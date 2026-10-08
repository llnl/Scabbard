
#include "sm_fixture.hpp"


TEST_F(SM_Tests, no_race) {
  auto tid = spawn_host_thread();
  std::uintptr_t mem = malloc_event(tid);
  host_write_event(tid, mem);
  auto jid = launch_event(tid, 0u);
  for (int i=0; i<20; ++i)
    device_read_event(jid, mem);
  device_write_event(jid, mem);
  device_job_completes_event(jid);
  sync_event(tid,0u);
  host_read_event(tid, mem);
  free_event(tid, mem);

  run();

  EXPECT_FALSE(any_issues()) << "[scabbard.rtl.sm.test:RPT] FAIL: no_race";
}

TEST_F(SM_Tests, def_HR_DW_race_0) {
  auto tid = spawn_host_thread();
  std::uintptr_t mem = malloc_event(tid);
  host_write_event(tid, mem);
  auto jid = launch_event(tid, 0u);
  for (int i=0; i<15; ++i)
    device_read_event(jid, mem);
  //sync_event(tid,0u); // not included before read and read before write -> true race must be caught
  host_read_event(tid, mem);
  for (int i=0; i<5; ++i)
    device_read_event(jid, mem);
  device_write_event(jid, mem);
  device_job_completes_event(jid);
  free_event(tid, mem);

  run();

  EXPECT_FALSE(any_true_race()) << "[scabbard.rtl.sm.test:RPT] FAIL: def_HR_DW_race_0";
}

TEST_F(SM_Tests, def_HR_DW_race_1) {
  auto tid = spawn_host_thread();
  std::uintptr_t mem = malloc_event(tid);
  host_write_event(tid, mem);
  auto jid = launch_event(tid, 0u);
  for (int i=0; i<20; ++i)
    device_read_event(jid, mem);
  device_write_event(jid, mem);
  device_job_completes_event(jid);
  //sync_event(tid,0u); // not included before read so min pos race must be caught
  host_read_event(tid, mem);
  free_event(tid, mem);

  run();

  EXPECT_FALSE(any_race()) << "[scabbard.rtl.sm.test:RPT] FAIL: def_HR_DW_race_1";
}



int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
