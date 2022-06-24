#include "robotiq_driver/robotiq_gripper_interface.hpp"
#include "robotiq_driver/crc.hpp"

#include <iostream>

constexpr int kBaudRate = 115200;
constexpr auto kTimeoutMilliseconds = 1000;

constexpr uint8_t kReadFunctionCode = 0x03;
constexpr uint16_t kFirstOutputRegister = 0x07D0;
constexpr uint8_t kNumOutputRegisters = 0x06;

constexpr uint8_t kWriteFunctionCode = 0x10;
constexpr uint16_t kActionRequestRegister = 0x03E8;

static uint8_t getFirstByte(uint16_t val)
{
  return (val & 0xFF00) >> 8;
}

static uint8_t getSecondByte(uint16_t val)
{
  return val & 0x00FF;
}

RobotiqGripperInterface::RobotiqGripperInterface(const std::string& com_port, uint8_t slave_id)
  : port_(com_port, kBaudRate, serial::Timeout::simpleTimeout(kTimeoutMilliseconds))
  , slave_id_(slave_id)
  , read_command_(createReadCommand(kFirstOutputRegister, kNumOutputRegisters))
{
  if (!port_.isOpen())
  {
    std::cerr << "Failed to open gripper port.\n";
    return;
  }
}

bool RobotiqGripperInterface::activateGripper()
{
  auto cmd = createWriteCommand(kActionRequestRegister, { 0x0100, 0x0000, 0x0000 }  // set rACT to 1, clear all
                                                                                    // other registers.
  );

  size_t num_bytes_written = port_.write(cmd);
  if (num_bytes_written != cmd.size())
  {
    std::cerr << "Attempted to write " << cmd.size() << " bytes, but only wrote " << num_bytes_written << ".\n";
    return false;
  }

  readResponse(8);

  return true;
}

void RobotiqGripperInterface::deactivateGripper()
{
  auto cmd = createWriteCommand(kActionRequestRegister, {0x0000, 0x0000, 0x0000});

  size_t num_bytes_written = port_.write(cmd);
  if (num_bytes_written != cmd.size()) {
    std::cerr << "Attempted to write " << cmd.size() << " bytes, but only wrote " << num_bytes_written << ".\n";
    return;
  }

  readResponse(8);
}

std::vector<uint8_t> RobotiqGripperInterface::createReadCommand(uint16_t first_register, uint8_t num_registers)
{
  std::vector<uint8_t> cmd = { slave_id_, kReadFunctionCode, getFirstByte(first_register),
                               getSecondByte(first_register), num_registers };
  auto crc = computeCRC(cmd.begin(), cmd.end());
  cmd.push_back(getFirstByte(crc));
  cmd.push_back(getSecondByte(crc));
  return cmd;
}

std::vector<uint8_t> RobotiqGripperInterface::createWriteCommand(uint16_t first_register,
                                                                 const std::vector<uint16_t>& data)
{
  uint16_t num_registers = data.size();
  uint8_t num_bytes = 2 * num_registers;

  std::vector<uint8_t> cmd = { slave_id_,
                               kWriteFunctionCode,
                               getFirstByte(first_register),
                               getSecondByte(first_register),
                               getFirstByte(num_registers),
                               getSecondByte(num_registers),
                               num_bytes };
  for (auto d : data)
  {
    cmd.push_back(getFirstByte(d));
    cmd.push_back(getSecondByte(d));
  }

  auto crc = computeCRC(cmd.begin(), cmd.end());
  cmd.push_back(getFirstByte(crc));
  cmd.push_back(getSecondByte(crc));

  return cmd;
}

std::vector<uint8_t> RobotiqGripperInterface::readResponse(size_t num_bytes_requested)
{
  std::vector<uint8_t> response;
  size_t num_bytes_read = port_.read(response, num_bytes_requested);

  if (num_bytes_read != num_bytes_requested)
  {
    std::cerr << "Requested " << num_bytes_requested << " bytes, but only got " << num_bytes_read << ".\n";
  }

  return response;
}