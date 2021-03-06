#include "pch.h"
#include "Protocol.h"
#include "WorkerThread.h"
#include "PacketManager.h"
#include "IOCPServer.h"
#include "Player.h"
#include "ThreadManager.h"
#include "RoomManager.h"
WorkerThread::WorkerThread()
{
}

WorkerThread::~WorkerThread()
{
}
void WorkerThread::Init()
{
	mythread = std::thread([&]() { WorkerThread::Proc(); });
	objectManager = ObjManager::GET_INSTANCE()->GetObjectManager();
}
void WorkerThread::Proc()
{
	while (true)
	{
		DWORD iosize;
		unsigned long long key;
		stOverEx *over;

		int ret = GetQueuedCompletionStatus(IOCPSERVER->GetIocp(), &iosize, &key,
			reinterpret_cast<WSAOVERLAPPED **>(&over), INFINITE);
		// 에러처리
		if (0 == ret) {
			int err_no = GetLastError();
			if (64 == err_no) {
				PACKETMANAGER->ClientDisconnect(key);
			}
			else error_display("GQCS : ", err_no);
			continue;
		}
		//접속종료 처리
		if (0 == iosize) {
			PACKETMANAGER->ClientDisconnect(key);
			continue;
		}

		if (OP_RECV == over->m_todo) {
			// 패킷 재조립
			int rest_data = iosize;
			unsigned char *ptr = over->m_IOCPbuf;
			int packet_size = 0;
			if (0 != objectManager->GetPlayer(key)->m_prev_size)
				packet_size = objectManager->GetPlayer(key)->m_packet_buf[0];

			while (rest_data > 0) {
				if (0 == packet_size) packet_size = ptr[0];
				int need_size = packet_size - objectManager->GetPlayer(key)->m_prev_size;
				if (rest_data >= need_size) {
					// 패킷을 조립 가능
					memcpy(objectManager->GetPlayer(key)->m_packet_buf
						+ objectManager->GetPlayer(key)->m_prev_size,
						ptr, need_size);

					//인게임
					if (objectManager->GetPlayer(key)->m_match == true) {
						objectManager->ProcessPacket(key, objectManager->GetPlayer(key)->m_packet_buf);
					}
					else { //매칭 관리
						objectManager->MatchProcess(key, objectManager->GetPlayer(key)->m_packet_buf);
					}
					//
					ptr += need_size;
					rest_data -= need_size;
					packet_size = 0;
					objectManager->GetPlayer(key)->m_prev_size = 0;
				}
				else {
					// 패킷을 완성할 수 없으니 남은 데이터 저장
					memcpy(objectManager->GetPlayer(key)->m_packet_buf
						+ objectManager->GetPlayer(key)->m_prev_size,
						ptr, rest_data);
					ptr += rest_data;
					objectManager->GetPlayer(key)->m_prev_size += rest_data;
					rest_data = 0;
				}
			}
			objectManager->OverlappedRecv(key);
		}
		else if (OP_SEND == over->m_todo) {
			delete over;
		}
		//else if (OP_STOP == over->m_todo) {
			//if (ROOMMANAGER->room[over->roomNum]->m_full == false) {
			//	//std::cout << over->roomNum << " : ROOM SYNC END" << std::endl;
			//	delete over;
			//}
		//}
		//else if (OP_ALLPOS == over->m_todo) {
			//PACKETMANAGER->AllPos(over->id);
			//std::cout << over->roomNum << " : ROOM SYNC" << std::endl;
			/*if(ROOMMANAGER->room[over->roomNum]->m_full == true)
				dynamic_cast<TimerThread*>(THREADMANAGER->FindThread(TIMER_TH))->AddTimer(over->id, OP_ALLPOS, over->roomNum, GetTickCount() + 4000);
			delete over;*/
		//}
		
	}
}
void WorkerThread::error_display(const char *msg, int err_no)
{
	WCHAR *lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << L"에러 " << lpMsgBuf << std::endl;
	while (true);
	LocalFree(lpMsgBuf);
}