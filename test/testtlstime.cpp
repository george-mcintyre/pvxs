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
    const certs::StatusDate date_now;
    const certs::StatusDate date_future;

    Tester()
        : now(time(nullptr)),
          future(now + ONE_DAY_OF_SECONDS),
          now_string(static_cast<certs::StatusDate>(now).s),
          future_string(certs::StatusDate(future).s),
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
        testEq(now, static_cast<certs::StatusDate>(date_now.s).t);
        testEq(future, static_cast<certs::StatusDate>(date_future.s).t);
        testEq(now_string, certs::StatusDate(date_now.t).s);
        testEq(future_string, certs::StatusDate(date_future.t).s);
    }

    void asn1_time() const {
        testShow() << __func__;
        const ossl_ptr<ASN1_TIME> now_asn1(ASN1_TIME_new());
        ASN1_TIME_set(now_asn1.get(), now);
        const ossl_ptr<ASN1_TIME> future_asn1(ASN1_TIME_new());
        ASN1_TIME_set(future_asn1.get(), future);

        testEq(now, static_cast<certs::StatusDate>(now_asn1).t);
        testEq(future, static_cast<certs::StatusDate>(future_asn1.get()).t);

        testEq(now, static_cast<certs::StatusDate>(date_now.toAsn1_Time().get()).t);
        testEq(future, static_cast<certs::StatusDate>(certs::StatusDate::toAsn1_Time(date_future).get()).t);
    }
};

void test_parseDuration() {
    testShow() << __func__;
    // Test basic durations
    testEq(impl::ConfigCommon::parseDuration("1y"), 365 * 24 * 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("4y"), 4 * 365 * 24 * 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("1M"), 30 * 24 * 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("1w"), 7 * 24 * 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("1d"), 24 * 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("1h"), 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("1m"), 60);
    testEq(impl::ConfigCommon::parseDuration("1s"), 1);

    // Test with whitespace and punctuation
    testEq(impl::ConfigCommon::parseDuration("1 y"), 365 * 24 * 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("1y, 6M"), 365 * 24 * 60 * 60 + 6 * 30 * 24 * 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("1d 12h"), 24 * 60 * 60 + 12 * 60 * 60);

    // Test combined durations
    testEq(impl::ConfigCommon::parseDuration("1y6M"),
           365 * 24 * 60 * 60 + 6 * 30 * 24 * 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("1d12h"),
           24 * 60 * 60 + 12 * 60 * 60);
    testEq(impl::ConfigCommon::parseDuration("1y6M30d12h30m45s"),
           365 * 24 * 60 * 60 +
           6 * 30 * 24 * 60 * 60 +
           30 * 24 * 60 * 60 +
           12 * 60 * 60 +
           30 * 60 +
           45);

    // Test unadorned numbers as minutes
    testEq(impl::ConfigCommon::parseDuration("5"), 5 * 60);
    testEq(impl::ConfigCommon::parseDuration("60"), 60 * 60);
    testEq(impl::ConfigCommon::parseDuration(" 30 "), 30 * 60);

    // Test error cases
    testEq(impl::ConfigCommon::parseDuration(""), -1);
    testEq(impl::ConfigCommon::parseDuration("abc"), -1);
    testEq(impl::ConfigCommon::parseDuration("1x"), -1);
    testEq(impl::ConfigCommon::parseDuration("y"), -1);
}
void test_formatDurationMins() {
    testShow() << __func__;

    // Test basic durations
    testEq(impl::ConfigCommon::formatDurationMins(525600), "1y"); // 365 days
    testEq(impl::ConfigCommon::formatDurationMins(2102400), "4y"); // 4 years
    testEq(impl::ConfigCommon::formatDurationMins(43200), "1M"); // 30 days = 1 month
    testEq(impl::ConfigCommon::formatDurationMins(10080), "1w"); // 1 week
    testEq(impl::ConfigCommon::formatDurationMins(1440), "1d"); // 1 day
    testEq(impl::ConfigCommon::formatDurationMins(60), "1h"); // 1 hour
    testEq(impl::ConfigCommon::formatDurationMins(1), "1m"); // 1 minute

    // Test complex durations
    testEq(impl::ConfigCommon::formatDurationMins(525600 + 6 * 43200), "1y 6M"); // 1 year 6 months
    testEq(impl::ConfigCommon::formatDurationMins(1440 + 12 * 60), "1d 12h"); // 1 day 12 hours
    testEq(impl::ConfigCommon::formatDurationMins(12 * 60 + 30), "12h 30m"); // 12 hours 30 minutes

    // Test round-trip conversion with a complex duration
    // When we parse "1y 6M 30d 12h 30m", it adds up to more than 1y 6M,
    // and actually becomes 1y 7M 12h 30m in total minutes
    std::string duration_str = "1y 6M 30d 12h 30m";
    int64_t minutes = impl::ConfigCommon::parseDurationMins(duration_str);
    std::string formatted = impl::ConfigCommon::formatDurationMins(minutes);

    // Since we expect the formatted result to be "1y 7M 12h 30m",
    // we'll test against that instead of the original string
    std::string expected = "1y 7M 12h 30m";
    testEq(formatted, expected);

    // Test duration with some zero values in between
    testEq(impl::ConfigCommon::formatDurationMins(525600 + 60), "1y 1h"); // 1 year 0 months 0 days 1 hour 0 minutes

    // Test with a calculated value that would have extra components
    // 1 year 6 months + 1 hour = 525,600 + 6 * 43,200 + 60 = 784,860
    testEq(impl::ConfigCommon::formatDurationMins(525600 + 6 * 43200 + 60), "1y 6M 1h");

    // Test with seconds - these would normally be truncated when converting to minutes
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1y 1s")), "1y");
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1h 59s")), "1h");

    // Test zero
    testEq(impl::ConfigCommon::formatDurationMins(0), "0m");

    // Additional round-trip tests
    // Simple values
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1y")), "1y");
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("6M")), "6M");
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("30d")), "1M");  // 30d gets converted to 1M for consistency
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1w")), "1w");
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1d")), "1d");
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1h")), "1h");
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1m")), "1m");

    // Combined values that should preserve well in round-trip
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1y 1M")), "1y 1M");
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1y 6M")), "1y 6M");
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("1d 12h")), "1d 12h");
    testEq(impl::ConfigCommon::formatDurationMins(
              impl::ConfigCommon::parseDurationMins("12h 30m")), "12h 30m");
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
