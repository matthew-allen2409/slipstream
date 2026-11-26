#include "csv_reader.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// Print error kinds by name in gtest failure messages instead of as raw integers.
namespace slipstream::replay {
inline void PrintTo(CsvErrorKind kind, std::ostream* os) { *os << to_string(kind); }
}  // namespace slipstream::replay

namespace {

using slipstream::replay::CsvError;
using slipstream::replay::CsvErrorKind;
using slipstream::replay::CsvReader;
using slipstream::replay::CsvRecord;

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

/// Writes `contents` byte for byte to a file unique to the current test, and removes it on exit.
class TempCsv {
public:
    explicit TempCsv(std::string_view contents) {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        // Parameterised test names contain '/' and quotes, so keep only filename-safe chars.
        std::string name = std::string{info->test_suite_name()} + "." + info->name();
        for (char& c : name) {
            const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                              (c >= '0' && c <= '9') || c == '.' || c == '_';
            if (!safe) {
                c = '_';
            }
        }
        m_path = (std::filesystem::path{::testing::TempDir()} /
                  (name + "." + std::to_string(s_counter++) + ".csv"))
                     .string();
        std::ofstream out{m_path, std::ios::binary | std::ios::trunc};
        out << contents;
    }
    ~TempCsv() {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }
    TempCsv(const TempCsv&) = delete;
    TempCsv& operator=(const TempCsv&) = delete;
    TempCsv(TempCsv&&) = delete;
    TempCsv& operator=(TempCsv&&) = delete;

    [[nodiscard]] const std::string& path() const noexcept { return m_path; }

private:
    static inline int s_counter = 0;
    std::string m_path;
};

/// A record with its fields copied out, so it stays valid after the next call to next().
struct OwnedRecord {
    std::size_t line;
    std::vector<std::string> fields;

    bool operator==(const OwnedRecord&) const = default;
    friend std::ostream& operator<<(std::ostream& os, const OwnedRecord& r) {
        os << "line " << r.line << ": [";
        for (std::size_t i = 0; i < r.fields.size(); ++i) {
            os << (i == 0 ? "" : ", ") << '"' << r.fields[i] << '"';
        }
        return os << ']';
    }
};

template <std::size_t N>
OwnedRecord own(const CsvRecord<N>& rec) {
    return OwnedRecord{.line = rec.line, .fields = {rec.fields.begin(), rec.fields.end()}};
}

/// Reads records until EOF or the first error. Stops after `limit` records so that a reader that
/// never reports EOF fails the test instead of hanging it.
struct ReadAllResult {
    std::vector<OwnedRecord> records;
    std::optional<CsvError> error;
};

template <std::size_t N>
ReadAllResult read_all(CsvReader<N>& reader, std::size_t limit = 100'000) {
    ReadAllResult out;
    for (std::size_t i = 0; i < limit; ++i) {
        auto rec = reader.next();
        if (!rec) {
            out.error = rec.error();
            return out;
        }
        if (!rec->has_value()) {
            return out;
        }
        out.records.push_back(own(**rec));
    }
    ADD_FAILURE() << "reader did not reach EOF within " << limit << " records";
    return out;
}

// ---------------------------------------------------------------------------------------------
// open()
// ---------------------------------------------------------------------------------------------

TEST(CsvReaderOpen, MissingFileIsFileOpenErrorAtLineZero) {
    auto reader = CsvReader<3>::open("/nonexistent/slipstream/definitely_missing.csv");
    ASSERT_FALSE(reader.has_value());
    EXPECT_EQ(reader.error().kind, CsvErrorKind::FileOpenError);
    EXPECT_EQ(reader.error().line, 0u);
}

TEST(CsvReaderOpen, ExistingFileOpens) {
    TempCsv csv{"a,b,c\n"};
    auto reader = CsvReader<3>::open(csv.path());
    EXPECT_TRUE(reader.has_value());
}

// ---------------------------------------------------------------------------------------------
// Splitting
// ---------------------------------------------------------------------------------------------

TEST(CsvReaderSplit, SplitsLineIntoFields) {
    TempCsv csv{"a,b,c\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    auto rec = reader->next();
    ASSERT_TRUE(rec.has_value());
    ASSERT_TRUE(rec->has_value());
    EXPECT_EQ(own(**rec), (OwnedRecord{1, {"a", "b", "c"}}));
}

TEST(CsvReaderSplit, ReturnsRecordsInOrderWithLineNumbers) {
    TempCsv csv{"a,b,c\nd,e,f\ng,h,i\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {1, {"a", "b", "c"}},
                                  {2, {"d", "e", "f"}},
                                  {3, {"g", "h", "i"}},
                              }));
}

TEST(CsvReaderSplit, EmptyFieldsAreEmptyViews) {
    // Quote rows in QandT.csv end in ",,," so trailing empty fields matter.
    TempCsv csv{",,\na,,c\na,,\n,,c\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {1, {"", "", ""}},
                                  {2, {"a", "", "c"}},
                                  {3, {"a", "", ""}},
                                  {4, {"", "", "c"}},
                              }));
}

TEST(CsvReaderSplit, DoesNotTrimWhitespace) {
    TempCsv csv{" a , b ,c \n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{{1, {" a ", " b ", "c "}}}));
}

TEST(CsvReaderSplit, SingleColumn) {
    TempCsv csv{"hello\nworld\n"};
    auto reader = CsvReader<1>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{{1, {"hello"}}, {2, {"world"}}}));
}

TEST(CsvReaderSplit, ShortLineAfterLongLineHasNoStaleData) {
    // The line buffer is reused; the second record must not see leftovers from the first.
    const std::string long_field(200, 'x');
    TempCsv csv{long_field + ",yyyyyyyy,zzzzzzzz\na,b,c\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {1, {long_field, "yyyyyyyy", "zzzzzzzz"}},
                                  {2, {"a", "b", "c"}},
                              }));
}

TEST(CsvReaderSplit, HeaderIsReturnedAsAnOrdinaryRecord) {
    // The reader knows nothing about headers; recognising one is the layer above's job.
    TempCsv csv{"Timestamp,Type,Symbol\n09:30:00.003,Q,SYNTH3\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {1, {"Timestamp", "Type", "Symbol"}},
                                  {2, {"09:30:00.003", "Q", "SYNTH3"}},
                              }));
}

// ---------------------------------------------------------------------------------------------
// Skipping comments and blank lines
// ---------------------------------------------------------------------------------------------

TEST(CsvReaderSkip, SkipsCommentLinesButCountsThem) {
    TempCsv csv{"# leading comment\na,b,c\n# middle comment\nd,e,f\n#\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {2, {"a", "b", "c"}},
                                  {4, {"d", "e", "f"}},
                              }));
}

TEST(CsvReaderSkip, SkipsBlankLinesButCountsThem) {
    TempCsv csv{"\na,b,c\n\n\nd,e,f\n\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {2, {"a", "b", "c"}},
                                  {5, {"d", "e", "f"}},
                              }));
}

TEST(CsvReaderSkip, HashOnlyMarksACommentAtTheStartOfALine) {
    // Decision encoded here: a comment is a line whose first character is '#'. A '#' anywhere
    // else (including after leading whitespace) is ordinary field data.
    TempCsv csv{"a,#b,c\n #x,y,z\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {1, {"a", "#b", "c"}},
                                  {2, {" #x", "y", "z"}},
                              }));
}

// ---------------------------------------------------------------------------------------------
// Line endings
// ---------------------------------------------------------------------------------------------

TEST(CsvReaderLineEndings, StripsTrailingCarriageReturn) {
    TempCsv csv{"a,b,c\r\nd,e,f\r\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {1, {"a", "b", "c"}},
                                  {2, {"d", "e", "f"}},
                              }));
}

TEST(CsvReaderLineEndings, CrlfBlankAndCommentLinesAreSkipped) {
    TempCsv csv{"\r\n# comment\r\na,b,c\r\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{{3, {"a", "b", "c"}}}));
}

TEST(CsvReaderLineEndings, LastLineWithoutNewlineIsStillReturned) {
    TempCsv csv{"a,b,c\nd,e,f"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {1, {"a", "b", "c"}},
                                  {2, {"d", "e", "f"}},
                              }));
}

// ---------------------------------------------------------------------------------------------
// EOF
// ---------------------------------------------------------------------------------------------

TEST(CsvReaderEof, EmptyFileIsImmediateEof) {
    TempCsv csv{""};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    auto rec = reader->next();
    ASSERT_TRUE(rec.has_value());
    EXPECT_FALSE(rec->has_value());
}

TEST(CsvReaderEof, FileOfOnlyCommentsAndBlanksIsEof) {
    TempCsv csv{"# one\n\n# two\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    auto rec = reader->next();
    ASSERT_TRUE(rec.has_value());
    EXPECT_FALSE(rec->has_value());
}

TEST(CsvReaderEof, EofIsRepeatable) {
    TempCsv csv{"a,b,c\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    auto first = reader->next();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(first->has_value());  // the record
    for (int i = 0; i < 3; ++i) {
        auto rec = reader->next();
        ASSERT_TRUE(rec.has_value()) << "call " << i;
        EXPECT_FALSE(rec->has_value()) << "call " << i;
    }
}

// ---------------------------------------------------------------------------------------------
// Field-count errors
// ---------------------------------------------------------------------------------------------

struct FieldCountCase {
    std::string_view line;
    CsvErrorKind kind;

    friend std::ostream& operator<<(std::ostream& os, const FieldCountCase& c) {
        return os << '"' << c.line << '"';
    }
};

class CsvReaderFieldCount : public ::testing::TestWithParam<FieldCountCase> {};

TEST_P(CsvReaderFieldCount, WrongFieldCountIsAnError) {
    const auto& param = GetParam();
    TempCsv csv{std::string{param.line} + "\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    auto rec = reader->next();
    ASSERT_FALSE(rec.has_value());
    EXPECT_EQ(rec.error().kind, param.kind);
    EXPECT_EQ(rec.error().line, 1u);
}

INSTANTIATE_TEST_SUITE_P(Cases, CsvReaderFieldCount,
                         ::testing::Values(FieldCountCase{"a", CsvErrorKind::TooFewFields},
                                           FieldCountCase{"a,b", CsvErrorKind::TooFewFields},
                                           FieldCountCase{",", CsvErrorKind::TooFewFields},
                                           FieldCountCase{"a,b,c,d", CsvErrorKind::TooManyFields},
                                           // A trailing comma starts a 4th (empty) field.
                                           FieldCountCase{"a,b,c,", CsvErrorKind::TooManyFields},
                                           FieldCountCase{",,,", CsvErrorKind::TooManyFields}));

TEST(CsvReaderErrors, OnlyOneTrailingCarriageReturnIsStripped) {
    // "c\r\r\n": strip one '\r' for CRLF, keep the other as data. Field count is still 3.
    TempCsv csv{"a,b,c\r\r\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{{1, {"a", "b", "c\r"}}}));
}

TEST(CsvReaderErrors, ErrorReportsPhysicalLineNumber) {
    TempCsv csv{"# comment\na,b,c\n\na,b\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{{2, {"a", "b", "c"}}}));
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->kind, CsvErrorKind::TooFewFields);
    EXPECT_EQ(result.error->line, 4u);
}

TEST(CsvReaderErrors, ErrorIsSticky) {
    // Decision encoded here: once next() fails, it keeps returning that same error, even though
    // later lines are valid. The caller is expected to abort; this makes "carry on anyway" safe.
    TempCsv csv{"a,b\nd,e,f\n"};
    auto reader = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(reader.has_value());

    for (int i = 0; i < 3; ++i) {
        auto rec = reader->next();
        ASSERT_FALSE(rec.has_value()) << "call " << i;
        EXPECT_EQ(rec.error().kind, CsvErrorKind::TooFewFields) << "call " << i;
        EXPECT_EQ(rec.error().line, 1u) << "call " << i;
    }
}

// ---------------------------------------------------------------------------------------------
// Move
// ---------------------------------------------------------------------------------------------

TEST(CsvReaderMove, MovedReaderContinuesFromSamePosition) {
    TempCsv csv{"a,b,c\nd,e,f\ng,h,i\n"};
    auto opened = CsvReader<3>::open(csv.path());
    ASSERT_TRUE(opened.has_value());
    CsvReader<3> first = std::move(*opened);

    auto rec = first.next();
    ASSERT_TRUE(rec.has_value() && rec->has_value());
    EXPECT_EQ(own(**rec), (OwnedRecord{1, {"a", "b", "c"}}));

    CsvReader<3> second = std::move(first);
    const auto result = read_all(second);
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.records, (std::vector<OwnedRecord>{
                                  {2, {"d", "e", "f"}},
                                  {3, {"g", "h", "i"}},
                              }));
}

// ---------------------------------------------------------------------------------------------
// to_string
// ---------------------------------------------------------------------------------------------

TEST(CsvErrorKindToString, EveryKindHasADistinctName) {
    const std::vector<CsvErrorKind> kinds{CsvErrorKind::FileOpenError, CsvErrorKind::ReadError,
                                          CsvErrorKind::TooFewFields,
                                          CsvErrorKind::TooManyFields};
    std::set<std::string_view> names;
    for (const auto kind : kinds) {
        const auto name = to_string(kind);
        EXPECT_FALSE(name.empty());
        EXPECT_NE(name, "Unknown");
        names.insert(name);
    }
    EXPECT_EQ(names.size(), kinds.size());
}

// ---------------------------------------------------------------------------------------------
// The real replay file
// ---------------------------------------------------------------------------------------------

TEST(CsvReaderRealFile, ReadsQandT) {
    auto reader = CsvReader<10>::open(SLIPSTREAM_DATA_DIR "/QandT.csv");
    ASSERT_TRUE(reader.has_value());

    const auto result = read_all(*reader);
    ASSERT_FALSE(result.error.has_value())
        << "line " << result.error->line << ": " << to_string(result.error->kind);

    // 20 comment lines, the header on line 21, then 10,000 data rows on lines 22..10021.
    ASSERT_EQ(result.records.size(), 10'001u);
    EXPECT_EQ(result.records.front().line, 21u);
    EXPECT_EQ(result.records.front().fields[0], "Timestamp");
    EXPECT_EQ(result.records.front().fields[9], "Aggressor");
    EXPECT_EQ(result.records.back().line, 10'021u);

    std::size_t quotes = 0;
    std::size_t trades = 0;
    for (std::size_t i = 1; i < result.records.size(); ++i) {
        const auto& rec = result.records[i];
        EXPECT_EQ(rec.line, result.records[i - 1].line + 1) << rec;
        if (rec.fields[1] == "Q") {
            ++quotes;
        } else if (rec.fields[1] == "T") {
            ++trades;
        } else {
            ADD_FAILURE() << "unexpected row type: " << rec;
        }
    }
    EXPECT_EQ(quotes, 6'905u);
    EXPECT_EQ(trades, 3'095u);
}

}  // namespace
