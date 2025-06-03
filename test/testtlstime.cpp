/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#define PVXS_ENABLE_EXPERT_API

#include <iostream>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <pvxs/log.h>
#include <pvxs/unittest.h>
#include <pvxs/config.h>

#include "certstatus.h"
#include "certdate.h"

namespace {
using namespace pvxs;

#define ONE_DAY_OF_SECONDS (60 * 60 * 24)

struct Tester {
    // Pristine values
    const time_t now{};
    const time_t future;
    const std::string now_string{};
    const std::string future_string{};

    // For testing Status date
    const certs::CertDate date_now;
    const certs::CertDate date_future;

    Tester()
        : now(time(nullptr)),
          future(now + ONE_DAY_OF_SECONDS),
          now_string(static_cast<certs::CertDate>(now).s),
          future_string(certs::CertDate(future).s),
          date_now(now),
          date_future(future) {
        testShow() << "Testing TLS Date Functions:\n";
    }

    ~Tester() = default;

    void initialisation() const {
        testShow() << __func__;
        testEq(now, date_now.t);
        testEq(future, date_future.t);
        testEq(now_string, date_now.s);
        testEq(future_string, date_future.s);
    }

    void conversion() const {
        testShow() << __func__;
        testEq(now, static_cast<certs::CertDate>(date_now.s).t);
        testEq(future, static_cast<certs::CertDate>(date_future.s).t);
        testEq(now_string, certs::CertDate(date_now.t).s);
        testEq(future_string, certs::CertDate(date_future.t).s);
    }

    void asn1_time() const {
        testShow() << __func__;
        const ossl_ptr<ASN1_TIME> now_asn1(ASN1_TIME_new());
        ASN1_TIME_set(now_asn1.get(), now);
        const ossl_ptr<ASN1_TIME> future_asn1(ASN1_TIME_new());
        ASN1_TIME_set(future_asn1.get(), future);

        testEq(now, static_cast<certs::CertDate>(now_asn1).t);
        testEq(future, static_cast<certs::CertDate>(future_asn1.get()).t);

        testEq(now, static_cast<certs::CertDate>(date_now.toAsn1_Time().get()).t);
        testEq(future, static_cast<certs::CertDate>(certs::CertDate::toAsn1_Time(date_future).get()).t);
    }
};

void test_parseDuration() {
    testShow() << __func__;
    const auto now = time(nullptr);
    // Test basic durations
    testEq(certs::CertDate::parseDuration("1y"), certs::CertDate::addCalendarUnits(now, 1) - now);
    testEq(certs::CertDate::parseDuration("4y"), certs::CertDate::addCalendarUnits(now, 4) - now);
    testEq(certs::CertDate::parseDuration("1M"), certs::CertDate::addCalendarUnits(now, 0, 1) - now);
    testEq(certs::CertDate::parseDuration("1w"), certs::CertDate::addCalendarUnits(now, 0,0, 7) - now);
    testEq(certs::CertDate::parseDuration("1d"), certs::CertDate::addCalendarUnits(now, 0,0, 1) - now);
    testEq(certs::CertDate::parseDuration("1h"), 60 * 60);
    testEq(certs::CertDate::parseDuration("1m"), 60);
    testEq(certs::CertDate::parseDuration("1s"), 1);

    // Test with whitespace and punctuation
    testEq(certs::CertDate::parseDuration("1 y"), certs::CertDate::addCalendarUnits(now, 1) - now);
    testEq(certs::CertDate::parseDuration("1y, 6M"), certs::CertDate::addCalendarUnits(now, 1, 6) - now);
    testEq(certs::CertDate::parseDuration("1d 12h"), 24 * 60 * 60 + 12 * 60 * 60);

    // Test combined durations
    testEq(certs::CertDate::parseDuration("1y6M"), certs::CertDate::addCalendarUnits(now, 1, 6) - now);
    testEq(certs::CertDate::parseDuration("1d12h"), 24 * 60 * 60 + 12 * 60 * 60);
    testEq(certs::CertDate::parseDuration("1y6M30d12h30m45s"),
        certs::CertDate::addCalendarUnits(now, 1, 6, 30) - now +
           12 * 60 * 60 +
           30 * 60 +
           45);

    // Test unadorned numbers as minutes
    testEq(certs::CertDate::parseDuration("5"), 5 * 60);
    testEq(certs::CertDate::parseDuration("60"), 60 * 60);
    testEq(certs::CertDate::parseDuration(" 30 "), 30 * 60);

    // Test error cases
    try {
        certs::CertDate::parseDuration("");
        testFail("Expected exception for empty duration string");
    } catch (const certs::CertTimeParseException &) {
        testPass("Empty duration string rejected");
    }

    try {
        certs::CertDate::parseDuration("abc");
        testFail("Expected exception for invalid duration format");
    } catch (std::runtime_error &) {
        testPass("Invalid duration format rejected");
    }
    try {
        certs::CertDate::parseDuration("1x");
        testFail("Expected exception for invalid unit in duration format");
    } catch (std::runtime_error &) {
        testPass("Invalid unit in duration format rejected");
    }
    try {
        certs::CertDate::parseDuration("y");
        testFail("Expected exception for unit without number in duration format");
    } catch (std::runtime_error &) {
        testPass("Invalid duration format for unit without number rejected");
    }
}

void test_formatDurationMins() {
    testShow() << __func__;
    const auto now = time(nullptr);

    // Test basic durations
    auto const one_year = (certs::CertDate::addCalendarUnits(now, 1, 0) - now) / 60;
    testEq(certs::CertDate::formatDurationMins(one_year), "1y");
    auto const four_years = (certs::CertDate::addCalendarUnits(now, 4, 0) - now)  / 60;
    testEq(certs::CertDate::formatDurationMins(four_years), "4y");
    auto const one_month = (certs::CertDate::addCalendarUnits(now, 0, 1) - now) / 60;
    testEq(certs::CertDate::formatDurationMins(one_month), "1M");
    testEq(certs::CertDate::formatDurationMins(10080), "7d"); // 7 days
    testEq(certs::CertDate::formatDurationMins(1440), "1d"); // 1 day
    testEq(certs::CertDate::formatDurationMins(60), "1h"); // 1 hour
    testEq(certs::CertDate::formatDurationMins(1), "1m"); // 1 minute

    // Test complex durations
    const auto one_year_and_six_months = (certs::CertDate::addCalendarUnits(now, 1, 6) - now) / 60;
    testEq(certs::CertDate::formatDurationMins(one_year_and_six_months), "1y 6M");
    testEq(certs::CertDate::formatDurationMins(1440 + 12 * 60), "1d 12h"); // 1 day 12 hours
    testEq(certs::CertDate::formatDurationMins(12 * 60 + 30), "12h 30m"); // 12 hours 30 minutes

    // Test round-trip conversion with a complex duration
    const std::string duration_str = "1y 6M 20d 12h 30m";
    const int64_t minutes = certs::CertDate::parseDurationMins(duration_str);
    const std::string formatted = certs::CertDate::formatDurationMins(minutes);

    testEq(formatted, duration_str);

    // Test duration with some zero values in between
    testEq(certs::CertDate::formatDurationMins(one_year + 60), "1y 1h"); // 1 year 0 months 0 days 1 hour 0 minutes

    // Test with a calculated value that would have extra components
    testEq(certs::CertDate::formatDurationMins(one_year_and_six_months + 60), "1y 6M 1h");

    // Test with seconds - these would normally be truncated when converting to minutes
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1y 1s")), "1y");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1h 59s")), "1h");

    // Test zero
    testEq(certs::CertDate::formatDurationMins(0), "0m");

    // Additional round-trip tests
    // Simple values
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1y")), "1y");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("6M")), "6M");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("27d")), "27d");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1w")), "7d");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1d")), "1d");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1h")), "1h");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1m")), "1m");

    // Combined values that should preserve well in round-trip
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1y 1M")), "1y 1M");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1y 6M")), "1y 6M");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("1d 12h")), "1d 12h");
    testEq(certs::CertDate::formatDurationMins( certs::CertDate::parseDurationMins("12h 30m")), "12h 30m");
}

}  // namespace

MAIN(testtlstime) {
    testPlan(60);  // Updated to match the actual number of tests
    testSetup();
    logger_config_env();
    Tester().initialisation();
    Tester().conversion();
    Tester().asn1_time();
    test_parseDuration();
    test_formatDurationMins();
    cleanup_for_valgrind();
    return testDone();
}
