#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Tooling.h>
#include <gtest/gtest.h>
#include "chimera/frontend_action.h"
#include "emulator.h"

using namespace chimera;
using namespace chimera::test;

//==============================================================================
TEST(Emulator, GeneralOptions)
{
    testing::internal::CaptureStdout();

    EXPECT_EXIT(Emulator::RunHelp(), ::testing::ExitedWithCode(0), ".*");
    EXPECT_EXIT(Emulator::Run("--help"), ::testing::ExitedWithCode(0), ".*");

    EXPECT_EXIT(Emulator::RunHelpList(), ::testing::ExitedWithCode(0), ".*");
    EXPECT_EXIT(Emulator::Run("--help-list"), ::testing::ExitedWithCode(0),
                ".*");

    EXPECT_EXIT(Emulator::RunVersion(), ::testing::ExitedWithCode(0), ".*");
    EXPECT_EXIT(Emulator::Run("--version"), ::testing::ExitedWithCode(0), ".*");

    std::string output = testing::internal::GetCapturedStdout();
}

//==============================================================================
TEST(Emulator, 01_Function)
{
    Emulator e;
    e.SetSource("01_function/function.h");
    e.SetConfigurationFile("01_function/function_pybind11.yaml");
    e.SetBinding("pybind11");

    // EXPECT_EXIT is necessary to continue to run subsequent tests, but it
    // doesn't stop at the breakpoints. For debugging use e.Run() instead.
    EXPECT_EXIT(e.Run(), ::testing::ExitedWithCode(0), ".*");
}

//==============================================================================
TEST(Emulator, 02_Class)
{
    Emulator e;
    e.SetSource("02_class/class.h");
    e.SetConfigurationFile("02_class/class.yaml");
    e.SetBinding("pybind11");

    // EXPECT_EXIT is necessary to continue to run subsequent tests, but it
    // doesn't stop at the breakpoints. For debugging use e.Run() instead.
    EXPECT_EXIT(e.Run(), ::testing::ExitedWithCode(0), ".*");
}

//==============================================================================
TEST(Emulator, 20_Eigen)
{
    Emulator e;
    e.SetSource("20_eigen/eigen.h");
    e.SetConfigurationFile("20_eigen/eigen_pybind11.yaml");
    e.SetBinding("pybind11");

    // EXPECT_EXIT is necessary to continue to run subsequent tests, but it
    // doesn't stop at the breakpoints. For debugging use e.Run() instead.
    e.Run();
    //    EXPECT_EXIT(e.Run(), ::testing::ExitedWithCode(0), ".*");
}
