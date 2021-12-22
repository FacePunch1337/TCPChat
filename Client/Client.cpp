﻿#pragma comment (lib, "Ws2_32.lib")

/*───────▄▀▀▀▀▀▀▀▀▀▀▄▄
────▄▀▀░░░░░░░░░░░░░▀▄
──▄▀░░░░░░░░░░░░░░░░░░▀▄
──█░░░░░░░░░░░░░░░░░░░░░▀▄
─▐▌░░░░░░░░▄▄▄▄▄▄▄░░░░░░░▐▌
─█░░░░░░░░░░░▄▄▄▄░░▀▀▀▀▀░░█
▐▌░░░░░░░▀▀▀▀░░░░░▀▀▀▀▀░░░▐▌
█░░░░░░░░░▄▄▀▀▀▀▀░░░░▀▀▀▀▄░█
█░░░░░░░░░░░░░░░░▀░░░▐░░░░░▐▌
▐▌░░░░░░░░░▐▀▀██▄░░░░░░▄▄▄░▐▌
─█░░░░░░░░░░░▀▀▀░░░░░░▀▀██░░█
─▐▌░░░░▄░░░░░░░░░░░░░▌░░░░░░█
──▐▌░░▐░░░░░░░░░░░░░░▀▄░░░░░█
───█░░░▌░░░░░░░░▐▀░░░░▄▀░░░▐▌
───▐▌░░▀▄░░░░░░░░▀░▀░▀▀░░░▄▀
───▐▌░░▐▀▄░░░░░░░░░░░░░░░░█
───▐▌░░░▌░▀▄░░░░▀▀▀▀▀▀░░░█
───█░░░▀░░░░▀▄░░░░░░░░░░▄▀
──▐▌░░░░░░░░░░▀▄░░░░░░▄▀
─▄▀░░░▄▀░░░░░░░░▀▀▀▀█▀
▀░░░▄▀░░░░░░░░░░▀░░░▀▀▀▀▄▄▄▄▄

░█████╗░██╗░░░░░██╗███████╗███╗░░██╗████████╗
██╔══██╗██║░░░░░██║██╔════╝████╗░██║╚══██╔══╝
██║░░╚═╝██║░░░░░██║█████╗░░██╔██╗██║░░░██║░░░
██║░░██╗██║░░░░░██║██╔══╝░░██║╚████║░░░██║░░░
╚█████╔╝███████╗██║███████╗██║░╚███║░░░██║░░░
░╚════╝░╚══════╝╚═╝╚══════╝╚═╝░░╚══╝░░░╚═╝░░░

*/

#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <wchar.h>
#include <stdio.h>
#include <list>
#include "../TCPChat/ChatMessage.h"

#define CMD_CONNECT_TO_SERVER  1001
#define CMD_SEND_MESSAGE  1002
#define SYNC_TIMER_MESSAGE 1003


const size_t MSG_LEN = 4096;
const size_t NIK_LEN = 16;

std::list<ChatMessage*> messages;
HINSTANCE hInst;
HWND grpEndpoint, grpLog, chatLog;
HWND btnSend, btnConnect, editMessage;
HWND editIP, editPort, editNickName;
char chatMsg[MSG_LEN];
char chatNik[NIK_LEN];
HANDLE sendLock = 0;
bool connection_to_server = false;
bool btn_check = false;
LRESULT CALLBACK  WinProc(HWND, UINT, WPARAM, LPARAM);
DWORD   CALLBACK  CreateUI(LPVOID);  // User Interface
DWORD   CALLBACK  SendChatMessage(LPVOID);
DWORD   CALLBACK  SyncChatMessage(LPVOID);
DWORD   CALLBACK  SendToServer(LPVOID);
bool DeserializeMessages(char*);

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_     PWSTR cmdLine, _In_     int showMode) {
	hInst = hInstance;

	const WCHAR WIN_CLASS_NAME[] = L"ClientWindow";
	WNDCLASS wc = { };
	wc.lpfnWndProc = WinProc;
	wc.hInstance = hInst;
	wc.lpszClassName = WIN_CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	ATOM mainWin = RegisterClassW(&wc);
	if (mainWin == FALSE) {
		MessageBoxW(NULL, L"Register class error", L"Register class error",
			 MB_OK | MB_ICONSTOP);
		return -1;
	}

	HWND hwnd = CreateWindowExW(0, WIN_CLASS_NAME,
		L"TCP Chat - Client", WS_OVERLAPPEDWINDOW,
		10, 100, 640, 480, NULL, NULL, hInst, NULL);
	if (hwnd == NULL) {
		MessageBoxW(NULL, L"Window create error",
			L"Window create error", MB_OK | MB_ICONSTOP);
		return -2;
	}

	ShowWindow(hwnd, showMode);
	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return 0;
}

LRESULT CALLBACK WinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:  
		CreateUI(&hWnd); 
		sendLock = CreateMutex(NULL, FALSE, NULL);
		if (sendLock == 0) {
			MessageBoxW(NULL, L"Mutex not created", L"App stopped", MB_OK | MB_ICONSTOP);
			PostQuitMessage(0);
			return 0;
		}
		SetTimer(hWnd, SYNC_TIMER_MESSAGE, 1000, NULL);
		break;
	case WM_DESTROY:
		KillTimer(hWnd, SYNC_TIMER_MESSAGE);
		CloseHandle(sendLock);
		PostQuitMessage(0);

		break;
	case WM_TIMER:
		if (wParam == SYNC_TIMER_MESSAGE) {
			CreateThread(NULL, 0, SyncChatMessage, &hWnd, 0, NULL);
		}
		break;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hWnd, &ps);
		FillRect(dc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_CTLCOLORSTATIC: {
		HDC  dc = (HDC)wParam;
		HWND ctl = (HWND)lParam;
		if (ctl != grpEndpoint
			&& ctl != grpLog) {
			SetBkMode(dc, TRANSPARENT);
			SetTextColor(dc, RGB(20, 20, 50));
		}
		return (LRESULT)GetStockObject(NULL_BRUSH);
	}
	case WM_COMMAND: {
		int cmd = LOWORD(wParam);
		int ntf = HIWORD(wParam);
		switch (cmd) {
		case CMD_SEND_MESSAGE: CreateThread(NULL, 0, SendChatMessage, &hWnd, 0, NULL); break;
		case CMD_CONNECT_TO_SERVER:
			
			connection_to_server = true;
			
			LRESULT state = SendMessageW(btnConnect, BM_GETCHECK, 0, 0);
			if (state == BST_CHECKED) {
				SendMessageW(btnConnect, BM_SETCHECK, BST_CHECKED, 0);
				SendMessageW(btnConnect, WM_KILLFOCUS, 0, 0);
				SendMessageW(btnConnect, WM_SETTEXT, 0, (LPARAM)L"disconnect");
			}
			else {
				SendMessageW(btnConnect, BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageW(btnConnect, WM_KILLFOCUS, 0, 0);
				SendMessageW(btnConnect, WM_SETTEXT, 0, (LPARAM)L"connect");

			}
			if (connection_to_server == true) {
				
				CreateThread(NULL, 0, SendToServer, &hWnd, 0, NULL);
				

			}
			
			
			
			
			EnableWindow(btnSend, TRUE);
			; 
			break;
			

		}
		break;
	}
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

DWORD CALLBACK CreateUI(LPVOID params) {
	HWND hWnd = *((HWND*)params);
	grpEndpoint = CreateWindowExW(0, L"Button", L"EndPoint", BS_GROUPBOX | WS_CHILD | WS_VISIBLE,
		10, 10, 150, 90, hWnd, 0, hInst, NULL);
	CreateWindowExW(0, L"Static", L"IP:", WS_CHILD | WS_VISIBLE,
		20, 35, 20, 15, hWnd, 0, hInst, NULL);
	editIP = CreateWindowExW(0, L"Edit", L"127.0.0.1", WS_CHILD | WS_VISIBLE,
		45, 35, 110, 17, hWnd, 0, hInst, NULL);
	CreateWindowExW(0, L"Static", L"Port:", WS_CHILD | WS_VISIBLE,
		20, 65, 30, 15, hWnd, 0, hInst, NULL);
	editPort = CreateWindowExW(0, L"Edit", L"8888", WS_CHILD | WS_VISIBLE,
		65, 65, 50, 17, hWnd, 0, hInst, NULL);
	btnConnect = CreateWindowW( L"Button", L"connect",  BS_CHECKBOX | BS_PUSHLIKE | WS_CHILD | WS_VISIBLE | DT_CENTER,
		10, 90, 95, 18, hWnd, (HMENU)CMD_CONNECT_TO_SERVER, hInst, NULL);
	editNickName = CreateWindowExW(0, L"Edit", L"nickname", WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTER,
		10, 130, 90, 23, hWnd, 0, hInst, NULL);
	grpLog = CreateWindowExW(0, L"Button", L"Chat log", BS_GROUPBOX | WS_CHILD | WS_VISIBLE,
		170, 10, 300, 300, hWnd, 0, hInst, NULL);
	chatLog = CreateWindowExW(0, L"Listbox", L"", WS_CHILD | WS_VISIBLE,
		180, 30, 280, 280, hWnd, 0, hInst, NULL);

	editMessage = CreateWindowExW(0, L"Edit", L"Hello, all", WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_BORDER,
		10, 150, 150, 50, hWnd, 0, hInst, NULL);
	btnSend = CreateWindowExW(0, L"Button", L"Send", WS_CHILD | WS_VISIBLE,
		40, 210, 100, 25, hWnd, (HMENU)CMD_SEND_MESSAGE, hInst, NULL);

	EnableWindow(btnSend, FALSE);
	return 0;
}

DWORD CALLBACK SendChatMessage(LPVOID params) {
	HWND hWnd = *((HWND*)params);

	

	int nikLen = SendMessageA(editNickName, WM_GETTEXT,
		NIK_LEN - 1, (LPARAM)chatNik);
	chatNik[nikLen] = '\0';

	int msgLen = SendMessageA(editMessage, WM_GETTEXT,
		MSG_LEN - NIK_LEN - 1, (LPARAM)chatMsg);
	chatMsg[msgLen] = '\0';

	strcat(chatMsg, "\t");
	strcat(chatMsg, chatNik);

	if (SendToServer(chatMsg) > 0)
		DeserializeMessages(chatMsg);
	else
		SendMessageA(chatLog, LB_ADDSTRING, 0, (LPARAM)"Sync error");
	
	return 0;
}

DWORD CALLBACK SyncChatMessage(LPVOID params) {
	HWND hWnd = *((HWND*)params);

	chatMsg[0] = '\0';
	if (SendToServer(chatMsg) > 0)
		DeserializeMessages(chatMsg);
	else
		SendMessageA(chatLog, LB_ADDSTRING, 0, (LPARAM)"Sync error");

	return 0;
}

DWORD   CALLBACK  SendToServer(LPVOID params) {
	char* data = (char*)params;
	if (data == NULL) return -1;

	SOCKET clientSocket;
	const size_t MAX_LEN = 100;
	WCHAR str[MAX_LEN];

	WSADATA wsaData;
	int err;

	WaitForSingleObject(sendLock, INFINITE);

	// unlock mutex
	


	// WinSock API initializing (~ wsaData = new WSA(2.2) )
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0) {
		_snwprintf_s(str, MAX_LEN, L"Startup failed, error %d", err);
		SendMessageW(chatLog, LB_ADDSTRING, 0, (LPARAM)str);
		return -10;
	}

	// Socket preparing
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET) {
		_snwprintf_s(str, MAX_LEN, L"Socket failed, error %d", WSAGetLastError());
		WSACleanup();
		SendMessageW(chatLog, LB_ADDSTRING, 0, (LPARAM)str);
		return -20;
	}

	// -- Socket configuration --
	SOCKADDR_IN addr;   // Config structure

	addr.sin_family = AF_INET;  // 1. Network type (family)

	char ip[20];
	LRESULT ipLen = SendMessageA(editIP, WM_GETTEXT, 19, (LPARAM)ip);
	ip[ipLen] = '\0';
	inet_pton(AF_INET, ip, &addr.sin_addr);  // 2. IP

	char port[8];
	LRESULT portLen = SendMessageA(editPort, WM_GETTEXT, 7, (LPARAM)port);
	port[portLen] = '\0';
	addr.sin_port = htons(atoi(port));  // 3. Port
	// -- end configuration of [addr] --

	// Connect to endpoint
	err = connect(clientSocket, (SOCKADDR*)&addr, sizeof(addr));
	if (err == SOCKET_ERROR) {
		_snwprintf_s(str, MAX_LEN,
			L"Socket connect error %d", WSAGetLastError());
		closesocket(clientSocket);
		WSACleanup();
		clientSocket = INVALID_SOCKET;
		SendMessageW(chatLog, LB_ADDSTRING, 0, (LPARAM)str);
		return -30;
	}

	const size_t chatMSG_LEN = 542;
	char editMsg[chatMSG_LEN];
	SendMessageA(editMessage, WM_GETTEXT, chatMSG_LEN, (LPARAM)editMsg);

	char username[16];
	SendMessageA(editNickName, WM_GETTEXT, 30, (LPARAM)username);

	const size_t MSG_LEN = 542;
	char message[MSG_LEN];

	_snprintf_s(message, MSG_LEN, MSG_LEN, "%s: %s", username, editMsg);

	SYSTEMTIME  time;
	GetLocalTime(&time);

	_snprintf_s(message, MSG_LEN, MSG_LEN, "%s    [%d:%d]", message, time.wHour, time.wMinute);
	
	int sent = send(clientSocket, message, MSG_LEN + 1, 0);
	if (sent == SOCKET_ERROR) {
		_snwprintf_s(str, MAX_LEN,
			L"Sending error %d", WSAGetLastError());
		closesocket(clientSocket);
		WSACleanup();
		clientSocket = INVALID_SOCKET;
		SendMessageW(chatLog, LB_ADDSTRING, 0, (LPARAM)str);
		return -40;
	}



	// receive in the same buffer - chatMsg
	int receivedCnt = recv(clientSocket, message, MSG_LEN - 1, 0);
	if (receivedCnt > 0) {
		
		editMsg[receivedCnt] = '\0';
		
		SendMessageA(chatLog, LB_ADDSTRING, 0, (LPARAM)message);
	}

		shutdown(clientSocket, SD_BOTH);
		closesocket(clientSocket);
		WSACleanup();

	SendMessageW(chatLog, LB_ADDSTRING, 0, (LPARAM)L"-End-");
	return 0;
}
