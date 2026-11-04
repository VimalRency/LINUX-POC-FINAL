#include <gtest/gtest.h>
#include "supervisor.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

class ConfigTest : public ::testing::Test {
protected:
    supervisor_t supervisor{};
    std::string config_path;
    std::string log_path;

    void SetUp() override {
        config_path = "/tmp/adas_supervisor_test_" + std::to_string(getpid()) + ".conf";
        log_path = "/tmp/adas_supervisor_test_" + std::to_string(getpid()) + ".log";
        ASSERT_EQ(supervisor_init(&supervisor, log_path.c_str()), 0);
    }

    void TearDown() override {
        supervisor_destroy(&supervisor);
        std::remove(config_path.c_str());
        std::remove(log_path.c_str());
    }

    void WriteConfig(const std::string& body) {
        std::ofstream out(config_path);
        out << body;
    }
};

TEST_F(ConfigTest, LoadsQuotedArgumentsAndDependencies) {
    WriteConfig(
        "base|2|1|-|/bin/sleep 30\n"
        "consumer|2|1|base|/usr/bin/printf \"hello world\"\n");
    ASSERT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), 0);
    process_t* consumer = supervisor_find_process(&supervisor, "consumer");
    ASSERT_NE(consumer, nullptr);
    ASSERT_EQ(consumer->dependency_count, 1U);
    EXPECT_STREQ(consumer->dependencies[0], "base");
    ASSERT_EQ(consumer->argc, 2U);
    EXPECT_STREQ(consumer->argv[1], "hello world");
}

TEST_F(ConfigTest, RejectsUnknownDependency) {
    WriteConfig("consumer|1|1|missing|/bin/sleep 1\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, RejectsDependencyCycle) {
    WriteConfig(
        "alpha|1|1|beta|/bin/sleep 1\n"
        "beta|1|1|alpha|/bin/sleep 1\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, RejectsDuplicateNames) {
    WriteConfig(
        "same|1|1|-|/bin/sleep 1\n"
        "same|1|1|-|/bin/sleep 2\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, RejectsUnterminatedQuote) {
    WriteConfig("broken|1|1|-|/usr/bin/printf \"missing quote\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}


TEST_F(ConfigTest, RejectsMissingConfigurationFile) {
    std::remove(config_path.c_str());
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, RejectsInvalidRestartCount) {
    WriteConfig("worker|invalid|1|-|/bin/sleep 1\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, RejectsInvalidRestartDelay) {
    WriteConfig("worker|1|invalid|-|/bin/sleep 1\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, RejectsEmptyCommand) {
    WriteConfig("worker|1|1|-|\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, RejectsEmptyProcessName) {
    WriteConfig("|1|1|-|/bin/sleep 1\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, RejectsMalformedFieldCount) {
    WriteConfig("worker|1|1|/bin/sleep 1\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, RejectsTooManyArguments) {
    std::string config = "worker|1|1|-|/usr/bin/printf";
    for (unsigned int index = 0U; index < SUPERVISOR_MAX_ARGS; ++index) {
        config += " arg";
    }
    config += "\n";
    WriteConfig(config);
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), -1);
}

TEST_F(ConfigTest, LoadsEscapedWhitespaceInArgument) {
    WriteConfig("worker|1|1|-|/usr/bin/printf hello\\ world\n");
    ASSERT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), 0);
    process_t* worker = supervisor_find_process(&supervisor, "worker");
    ASSERT_NE(worker, nullptr);
    ASSERT_EQ(worker->argc, 2U);
    EXPECT_STREQ(worker->argv[1], "hello world");
}

TEST_F(ConfigTest, IgnoresCommentsAndBlankLines) {
    WriteConfig("\n# supervisor test configuration\nworker|1|1|-|/bin/sleep 1\n\n");
    EXPECT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), 0);
    EXPECT_NE(supervisor_find_process(&supervisor, "worker"), nullptr);
}

TEST_F(ConfigTest, DependencyIsNotReadyUntilRunning) {
    WriteConfig(
        "base|1|1|-|/bin/sleep 10\n"
        "consumer|1|1|base|/bin/sleep 10\n");
    ASSERT_EQ(supervisor_load_config(&supervisor, config_path.c_str()), 0);
    process_t* base_process = supervisor_find_process(&supervisor, "base");
    process_t* consumer = supervisor_find_process(&supervisor, "consumer");
    ASSERT_NE(base_process, nullptr);
    ASSERT_NE(consumer, nullptr);

    base_process->state = PROCESS_STOPPED;
    base_process->pid = -1;
    EXPECT_EQ(supervisor_dependencies_ready(&supervisor, consumer), 0);

    base_process->state = PROCESS_RUNNING;
    base_process->pid = 12345;
    EXPECT_EQ(supervisor_dependencies_ready(&supervisor, consumer), 1);

    base_process->pid = -1;
    base_process->state = PROCESS_STOPPED;
}

TEST(SupervisorApiTest, HandlesNullArgumentsSafely) {
    EXPECT_EQ(supervisor_load_config(nullptr, "missing.conf"), -1);
    EXPECT_EQ(supervisor_start_all(nullptr), -1);
    EXPECT_EQ(supervisor_find_process(nullptr, "worker"), nullptr);
    EXPECT_EQ(supervisor_find_process(nullptr, nullptr), nullptr);
    EXPECT_EQ(supervisor_dependencies_ready(nullptr, nullptr), 0);
    supervisor_destroy(nullptr);
    supervisor_stop_all(nullptr);
    supervisor_mark_shutdown(nullptr);
    supervisor_print_status(nullptr);
}
