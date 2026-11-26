#pragma once

/// @file
/// @brief Line-oriented CSV tokenizer for the replay clients.
///
/// CsvReader only splits lines into fields. It does not know about headers, column meanings or
/// types; turning fields into Quote / Trade messages is the job of the layer above it.

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace slipstream::replay {

/// @brief One data line, split into exactly `Columns` fields.
///
/// The fields point into the reader's internal buffers. They are valid only until the next
/// call to CsvReader::next() on the same reader, or until that reader is moved or destroyed.
/// Copy out anything that must live longer.
template <std::size_t Columns>
    requires(Columns > 0)
struct CsvRecord {
    std::size_t line;  ///< 1-based physical line number (comments and blanks count).
    std::span<const std::string_view, Columns> fields;  ///< Raw field text, untrimmed.
};

/// @brief Why CsvReader could not produce a record.
enum class CsvErrorKind : std::uint8_t {
    FileOpenError = 0,  ///< The file could not be opened.
    ReadError,          ///< The stream failed for a reason other than end of file.
    TooFewFields,       ///< The line has fewer than `Columns` fields.
    TooManyFields,      ///< The line has more than `Columns` fields.
};

/// @brief Name of a CsvErrorKind, for log messages.
[[nodiscard]] constexpr std::string_view to_string(CsvErrorKind kind) {
    switch (kind) {
        case CsvErrorKind::FileOpenError:
            return "FileOpenError";
        case CsvErrorKind::ReadError:
            return "ReadError";
        case CsvErrorKind::TooFewFields:
            return "TooFewFields";
        case CsvErrorKind::TooManyFields:
            return "TooManyFields";
    }

    return "Unknown";
}

/// @brief A CsvReader failure and where it happened.
struct CsvError {
    std::size_t line;   ///< 1-based line; 0 for FileOpenError; approximate for ReadError.
    CsvErrorKind kind;  ///< What went wrong.
};

/// @brief Reads a CSV file one record at a time, splitting each line into `Columns` fields.
///
/// Line handling:
/// - A single trailing '\\r' is stripped, so CRLF files read the same as LF files.
/// - Blank lines, and lines whose first character is '#', are skipped but still counted.
/// - Every other line must have exactly `Columns` comma-separated fields. Fields are not
///   trimmed and quoting is not supported; a trailing comma adds an empty field.
///
/// Errors are sticky: after next() fails, every later call returns the same error.
/// The reader owns its file and is move-only.
///
/// @tparam Columns Number of fields every data line must have.
template <std::size_t Columns>
    requires(Columns > 0)
class CsvReader {
public:
    /// @brief Opens `path` for reading.
    /// @return The reader, or a CsvError with kind FileOpenError and line 0.
    [[nodiscard]] static std::expected<CsvReader, CsvError> open(const std::string& path) {
        std::ifstream stream{path};

        if (stream.is_open()) {
            return CsvReader<Columns>(std::move(stream));
        }

        return std::unexpected{CsvError{.line = 0, .kind = CsvErrorKind::FileOpenError}};
    }

    /// @brief Reads up to and including the next data line.
    ///
    /// Skips blank and comment lines until it reaches a data line, end of file, or an
    /// error.
    /// The returned record is invalidated by the next call (see CsvRecord).
    ///
    /// @return The next record; `std::nullopt` at end of file (and on every call after it);
    ///         or a CsvError (ReadError, TooFewFields, TooManyFields).
    [[nodiscard]] std::expected<std::optional<CsvRecord<Columns>>, CsvError> next() {
        if (m_err) [[unlikely]] {
            return std::unexpected{*m_err};
        }

        while (std::getline(m_file_stream, m_line)) {
            ++m_line_number;

            if (!m_line.empty() && m_line.back() == '\r') {
                m_line.pop_back();
            }

            if (m_line.empty() || m_line.starts_with('#')) {
                continue;
            }

            return split(m_line);
        }

        if (m_file_stream.bad()) {
            m_err = CsvError{.line = m_line_number + 1, .kind = CsvErrorKind::ReadError};
            return std::unexpected{*m_err};
        }

        return std::nullopt;
    }

private:
    /// @brief Takes ownership of an already-open stream. Use open() instead.
    explicit CsvReader(std::ifstream&& stream) : m_file_stream{std::move(stream)} {}

    /// @brief Splits `str` (a view of m_line) on ',' into m_fields.
    ///
    /// Stops as soon as a field beyond `Columns` is found, so m_fields is never overrun.
    /// Any error is also stored in m_err so it stays sticky.
    [[nodiscard]] std::expected<CsvRecord<Columns>, CsvError> split(std::string_view str) {
        std::size_t tokens{0};
        while (true) {
            if (tokens == Columns) {
                m_err = CsvError{.line = m_line_number, .kind = CsvErrorKind::TooManyFields};
                return std::unexpected{*m_err};
            }

            auto pos{str.find(',')};

            m_fields[tokens] = str.substr(0, pos);

            ++tokens;
            if (pos == std::string_view::npos) {
                break;
            }

            str.remove_prefix(pos + 1);
        }

        if (tokens < Columns) {
            m_err = CsvError{.line = m_line_number, .kind = CsvErrorKind::TooFewFields};
            return std::unexpected{*m_err};
        }

        return CsvRecord<Columns>{.line = m_line_number, .fields = m_fields};
    }

    /// The open file.
    std::ifstream m_file_stream;
    /// Last line successfully read (0 before the first read).
    std::size_t m_line_number{0};
    /// Current line, with any '\\r' stripped. m_fields view into it.
    std::string m_line{};
    /// Storage behind CsvRecord::fields.
    std::array<std::string_view, Columns> m_fields;
    /// Set once next() has failed; returned by every later call.
    std::optional<CsvError> m_err{std::nullopt};
};

}  // namespace slipstream::replay
