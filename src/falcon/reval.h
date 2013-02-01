#ifndef REVAL_H_
#define REVAL_H_

// Definitions for the register compiler/evaluator.
#include "frameobject.h"
#include "util.h"
#include <algorithm>
#include <set>

#define REG_MAX_STACK 256
#define REG_MAX_FRAMES 32
#define REG_MAX_BBS 1024

#define REG_MAGIC "REG1"

#ifdef SWIG
#define f_inline
#else
#define f_inline __attribute__((always_inline)) inline
#endif

bool is_varargs_op(int opcode);
bool is_branch_op(int opcode);

// Register layout --
//
// Each function call pushes a new set of registers for evaluation.
//
// The first portion of the register file is aliased to the function
// locals and consts.  This is followed by general registers.
// [0..#consts][0..#locals][general registers]
typedef int16_t Register;
typedef int16_t JumpLoc;
typedef void* JumpAddr;

#define INCREF 148
#define DECREF 149

#pragma pack(push, 0)
struct RegisterPrelude {
  int32_t magic;
  int16_t num_registers;
  int16_t mapped_labels :1;
  int16_t mapped_registers :1;
  int16_t reserved :14;
};

struct OpHeader {
  uint64_t code :8;
  uint64_t arg :14;
};

struct BranchOp {
  uint64_t code :8;
  uint64_t arg :14;
  uint64_t reg_1 :14;
  uint64_t reg_2 :14;
  uint64_t label :14;
};

struct RegOp {
  uint64_t code :8;
  uint64_t arg :14;
  uint64_t reg_1 :14;
  uint64_t reg_2 :14;
  uint64_t reg_3 :14;
};

// A variable size op is at least 8 bytes, but can contain
// addition registers off the end of the structure.
struct VarRegOp {
  uint32_t code :8;
  uint32_t arg :14;
  uint32_t num_registers :10;
  Register regs[2];
};

struct RMachineOp {
  union {
    OpHeader header;
    BranchOp branch;
    RegOp reg;
    VarRegOp varargs;
  };

  inline uint8_t code() {
    return header.code;
  }

  inline uint8_t arg() {
    return header.arg;
  }

  inline int size() {
    return RMachineOp::size(*this);
  }

  static f_inline Py_ssize_t size(const VarRegOp& op) {
    return sizeof(RMachineOp) + sizeof(Register) * std::max(0, op.num_registers - 2);
  }

  static f_inline Py_ssize_t size(const RegOp& op) {
    return sizeof(RMachineOp);
  }

  static f_inline Py_ssize_t size(const BranchOp& op) {
    return sizeof(RMachineOp);
  }

  static f_inline Py_ssize_t size(const RMachineOp& op) {
    assert(sizeof(RMachineOp) == 8);
    assert(sizeof(VarRegOp) == sizeof(RMachineOp));

    if (is_varargs_op(op.header.code)) {
      return size(op.varargs);
    }
    if (is_branch_op(op.header.code)) {
      return sizeof(RMachineOp);
    }
    return sizeof(RMachineOp);
  }
};

#pragma pack(pop)

struct RegisterFrame {
  PyFrameObject* frame;
  PyObject* regcode;
  RegisterFrame(PyFrameObject* f, PyObject* c) :
      frame(f), regcode(c) {
  }
  ~RegisterFrame() {
    Py_XDECREF(frame);
  }
};

class Evaluator {
private:
  f_inline void collectInfo(int opcode);

public:
  int32_t opCounts[256];
  int64_t opTimes[256];

  int32_t totalCount;
  int64_t lastClock;

  Evaluator() {
    bzero(opCounts, sizeof(opCounts));
    bzero(opTimes, sizeof(opTimes));
    totalCount = 0;
    lastClock = 0;
  }

  PyObject* eval(RegisterFrame* rf);
  PyObject* evalPython(PyObject* func, PyObject* args);

  RegisterFrame* buildFrameFromPython(PyObject* func, PyObject* args);
  RegisterFrame* buildFrameFromRegCode(PyObject* code);

  void dumpStatus();
};

#endif /* REVAL_H_ */