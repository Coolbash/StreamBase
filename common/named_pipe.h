#pragma once

#include "global_types.h"
#include <iostream>


///class for keeping a pipe handle and working with it
///this class works specifically for this project and doesn't purposes to be used in another projects.
class CNamedPipeClient
{
public:
	CNamedPipeClient() : m_handle{ INVALID_HANDLE_VALUE } {}; ///< default constructor
	CNamedPipeClient(CNamedPipeClient const&) = delete;		///< disabling copy constructor
	CNamedPipeClient(CNamedPipeClient&& src) noexcept { m_handle = src.m_handle; src.m_handle = INVALID_HANDLE_VALUE; } ///< move consturcor
	~CNamedPipeClient() { close(); } ///< destructor that closes the handle

	void close()	///< closes the handle
	{
		disconnect();
		if (m_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_handle);
			m_handle = INVALID_HANDLE_VALUE;
		}
	}

	void disconnect() ///< disconnects the pipe but doesn't close it.
	{
		if (m_handle != INVALID_HANDLE_VALUE)
		{
			FlushFileBuffers(m_handle);
			DisconnectNamedPipe(m_handle);
		}
	}

	bool open() ///< opens the pipe
	{
		for(int tries=10; tries; --tries)
		{
			m_handle = CreateFile(PipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if (m_handle != INVALID_HANDLE_VALUE)
			{
				DWORD dwMode = PIPE_READMODE_MESSAGE;
				DWORD buf_size{ max_message_length };
				DWORD timeout{ 1000 };
				SetNamedPipeHandleState(m_handle, &dwMode, &buf_size, &timeout); //ignore errors.
				return true;
			}
			else
			{
				auto error = GetLastError();
				switch (error)
				{
				case ERROR_PIPE_BUSY:	WaitNamedPipe(PipeName, 10000); break; // the pipe is busy. wait a little.
				case ERROR_FILE_NOT_FOUND: Sleep(1000); break; //there is no pipe yet. The server isn't started yet. wait a little
				default:
					std::cerr << "error opening named pipe. Error code " << error << '\n';
					return false;
				}
			}
		}
		// we tried many times. No success
		std::cout << "can't open the pipe (no server is runing?)\n";
		return false; 
	}

	DWORD receive(void const* data, DWORD const size) ///< receives data from the pipe to the buffer
	{
		DWORD read_size{ 0 };
		if (ReadFile(m_handle, const_cast<LPVOID>(data), size, &read_size, nullptr))
			return read_size;
		switch (auto errorcode = GetLastError())
		{
		case ERROR_MORE_DATA:   std::cout << "there is more data in the pipe. Try to increase buffer or read again\n"; break;
		case ERROR_BROKEN_PIPE: std::cout << "the pipe is closed.\n"; break;
		default:
			std::cerr << "error reading from the pipe. error code " << errorcode << '\n';
		}
		return read_size;
	}

	//sending routines
	DWORD send(void const* data, DWORD const size) ///< sends raw data
	{
		if(size < max_message_length)
		{
			DWORD sent_size{ 0 };
			if (WriteFile(m_handle, const_cast<LPVOID>(data), size, &sent_size, nullptr))
				return sent_size;
			std::cerr << "error writing to the pipe. error code " << GetLastError() << '\n';
		}
		else
			std::cerr << "error: asking for sending too big message.\n";
		return 0;
	}

	template<typename T> bool send_struct(T const data) { return send(&data, sizeof(data)) == sizeof(data); } ///< sends any type of known size
	template<typename T> bool send_container(T const container, msg_type type) ///< sends a container like string or vector
	{
		DWORD const send_size = static_cast<DWORD>(sizeof(msg_type) + sizeof(DWORD) + container.size() * sizeof(std::string::value_type));
		auto tmp = std::make_unique<byte[]>(send_size);
		auto b = reinterpret_cast<blob_t<std::string::value_type>*>(tmp.get());
		b->t = type;
		b->size = static_cast<DWORD>(container.size());
		memcpy(&b->data, container.data(), container.size());
		return send(tmp.get(), send_size) == send_size;
	}

	bool send(msg_type msg) { return send_struct(msg); } ///< sends a simple request/command without any data
	template<typename T>bool send(T const & data, msg_type msg) { return send_struct(simple_t<T>{msg, data}); }  ///< sends data with simple type (fixed size)
	bool send(std::string_view s, msg_type msg) { return send_container(s, msg); }		///< sends a string

	DWORD bytesForRead() ///< returns number of bytes waiting in the pipe
	{
		DWORD result{ 0 };
		PeekNamedPipe(m_handle, nullptr, 0, nullptr, &result, nullptr);
		return result;
	}

protected:
	HANDLE m_handle = INVALID_HANDLE_VALUE; ///< the handle to the pipe
};


///server side version of the class
class CNamedPipeServer : public CNamedPipeClient
{
public:
	bool create()	///< creates the pipe
	{
		if (m_handle == INVALID_HANDLE_VALUE)
		{
			m_handle = CreateNamedPipe(PipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, max_connections, max_message_length, max_message_length, 0, nullptr);
			if (m_handle == INVALID_HANDLE_VALUE)
			{
				std::cerr << "error creating named pipe. Error code " << GetLastError() << '\n';
				return false;
			}
		}
		return true;
	};

	bool	wait_connection() ///< waits connection
	{
		if (ConnectNamedPipe(m_handle, nullptr))
			return true;
		std::cerr << "error connecting named pipe. Error code " << GetLastError() << '\n';
		return false;
	}
};


