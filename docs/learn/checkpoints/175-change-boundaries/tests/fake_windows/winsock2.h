#pragma once
#include "windows.h"
using SOCKET=std::uintptr_t;
struct WSABUF { ULONG len; char* buf; };
using WSAOVERLAPPED=OVERLAPPED;
constexpr int WSA_IO_PENDING=997,WSAECONNRESET=10054;
int WSAGetLastError();
int WSARecv(SOCKET,WSABUF*,DWORD,DWORD*,DWORD*,OVERLAPPED*,void*);
