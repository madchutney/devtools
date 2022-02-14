/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrTestEnv.h"
#include "RteFsUtils.h"
#include "CrossPlatformUtils.h"

using namespace std;

string testinput_folder;
string testoutput_folder;
string testcmsispack_folder;
string schema_folder;

CoutRedirect::CoutRedirect() :
  m_buffer(""), m_oldStreamBuf(std::cout.rdbuf(m_buffer.rdbuf()))
{
}

std::string CoutRedirect::GetString() {
  return m_buffer.str();
}

CoutRedirect::~CoutRedirect() {
  // reverse redirect
  std::cout.rdbuf(m_oldStreamBuf);
}

void ProjMgrTestEnv::SetUp() {
  error_code ec;
  schema_folder        = string(TEST_FOLDER) + "../schemas";
  testinput_folder     = string(TEST_FOLDER) + "data";
  testoutput_folder    = RteFsUtils::GetCurrentFolder() + "output";
  testcmsispack_folder = string(CMAKE_SOURCE_DIR) + "test/packs";
  if (RteFsUtils::Exists(testoutput_folder)) {
    RteFsUtils::RemoveDir(testoutput_folder);
  }
  RteFsUtils::CreateDirectories(testoutput_folder);
  fs::copy(fs::path(schema_folder), fs::path(testoutput_folder + "/schemas"), fs::copy_options::recursive, ec);
  schema_folder     = testoutput_folder + "/schemas";
  testinput_folder  = fs::canonical(testinput_folder).generic_string();
  testoutput_folder = fs::canonical(testoutput_folder).generic_string();
  testoutput_folder = fs::canonical(testoutput_folder).generic_string();
  schema_folder     = fs::canonical(schema_folder).generic_string();
  ASSERT_FALSE(testinput_folder.empty());
  ASSERT_FALSE(testoutput_folder.empty());
  ASSERT_FALSE(schema_folder.empty());
  CrossPlatformUtils::SetEnv("CMSIS_PACK_ROOT", testcmsispack_folder);
}

void ProjMgrTestEnv::TearDown() {
  // Reserved
}

int main(int argc, char **argv) {
  try {
    testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new ProjMgrTestEnv);
    return RUN_ALL_TESTS();
  }
  catch (testing::internal::GoogleTestFailureException const& e) {
    cout << "runtime_error: " << e.what();
    return 2;
  }
  catch (...) {
    cout << "non-standard exception";
    return 2;
  }
}
