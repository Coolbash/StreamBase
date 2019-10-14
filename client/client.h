#pragma once

#include "../common/global_types.h"
#include "../common/named_pipe.h"
#include <string>
#include <optional>


/// CLient in clien app is used for keeping all objects needed for communication.
class CClient
{
public:
	bool open_pipe() { return m_pipe.open(); }		///< just opens a pipe
	bool dispatch_received_data();					///< reads input buffer and make decisions
	bool receive_and_dispatch();					///< reads data from the pipe and sends data to dispatch
	template<typename T> std::optional<T>receive_simple_data(); ///< if you waiting for specific simple data; optional diesn't set if data isn't read
	std::optional<std::string_view> recieve_string(); ///< if you waiting a string from the pipe.
	bool communicate_with_object(DWORD id);			///< just plays with remote object
	bool communicate_with_object_int(DWORD id);		///< plays with specific remote object
	bool communicate_with_object_double(DWORD id);	///< plays with specific remote object

	bool communicate();	///< test all possible way to communicate with a server

private:
	CNamedPipeClient m_pipe;					///< the pipe
	message_t		m_buf;						///< the buffer for reading from the pipe.
	std::string		m_server_description;		///< description string, received from the server
	DWORD			m_ID = 0;					///< number of this client received from the server
	bool			m_version_ok = false;		///< indicates if protocol versions of the server and the client are correspond
	static std::string_view m_description;		///< description of the client proccess for sending to server
};
