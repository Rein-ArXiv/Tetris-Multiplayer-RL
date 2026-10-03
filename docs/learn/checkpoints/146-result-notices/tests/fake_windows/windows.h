#pragma once
// Behavioral test double declarations, NOT Windows ABI or OS emulation.
#include <cstdint>
#include <cstddef>
using DWORD=std::uint32_t;using ULONG=std::uint32_t;using ULONG_PTR=std::uintptr_t;
using BOOL=int;using HANDLE=void*;
struct OVERLAPPED { std::uintptr_t Internal=0,InternalHigh=0; DWORD Offset=0,OffsetHigh=0; HANDLE hEvent=nullptr; };
constexpr BOOL FALSE=0,TRUE=1;
constexpr DWORD INFINITE=0xffffffffu,WAIT_TIMEOUT=258,ERROR_SUCCESS=0,ERROR_INVALID_PARAMETER=87,
 ERROR_INVALID_DATA=13,ERROR_OPERATION_ABORTED=995,ERROR_NOT_FOUND=1168,ERROR_INVALID_HANDLE=6;
HANDLE CreateIoCompletionPort(HANDLE,HANDLE,ULONG_PTR,DWORD);
BOOL CloseHandle(HANDLE);
DWORD GetLastError();
BOOL GetQueuedCompletionStatus(HANDLE,DWORD*,ULONG_PTR*,OVERLAPPED**,DWORD);
BOOL PostQueuedCompletionStatus(HANDLE,DWORD,ULONG_PTR,OVERLAPPED*);
BOOL CancelIoEx(HANDLE,OVERLAPPED*);
