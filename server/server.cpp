// server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "../common/global_types.h"
#include "../common/named_pipe.h"
#include "distributed.h"
#include <iostream>
#include <array>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <map>
#include <optional>


//global variables
std::atomic<int> waiting_pipes(0); ///< quantity of waiting pipes.
volatile bool flag_shutdown{ false }; ///< a flag to all threads to close
			
///a class that represents a remote client and keeps all created for it objects
class CRemoteClient
{
public:
	CRemoteClient() { m_client_ID = std::atomic_fetch_add(&client_number, 1); };

	int create(distributed_classes object_type)	///< creates an object of type number t
	{
		DWORD result{ 0 };
		std::unique_ptr<distributed> object;
		switch (object_type)
		{
		case distributed_classes::distributed_int:		object = std::make_unique<distributed_int>(m_client_ID);	break;
		case distributed_classes::distributed_double:	object = std::make_unique<distributed_double>(m_client_ID);break;
		default: std::cerr << "error: asking to create an object of unsupported class";
		}
		if(object)
		{
			result = object->get_ID();
			objects[result] = std::move(object);
		}
		return result;
	}

	bool destroy(int object_ID) ///< destroys the object with ID 
	{
		return objects.erase(object_ID);
	}

	/// calls a method number method of the object object_ID with parameters (param)
	void execute(DWORD object_ID, distributed_methods method, void const* param, CNamedPipeServer & pipe) 
	{
		try
		{
			auto & object = objects.at(object_ID);
			switch (method)
			{
			case	distributed_methods::get_class_name:
			{
				auto result = object->get_class_name();
				pipe.send_container(result, msg_type::method_result);
				break;
			}
			case	distributed_methods::get_instance:
			{
				auto result = object->get_ID();
				pipe.send(result, msg_type::method_result);
				break;
			}
			case	distributed_methods::get_client:
			{
				auto result = object->get_client_ID();
				pipe.send(result, msg_type::method_result);
				break;
			}
			case	distributed_methods::set_param:
			{
				object->set_param(*reinterpret_cast<simple_variant_t const*>(param));
				break;
			}
			case	distributed_methods::get_param:
			{
				auto result = object->get_param();
				pipe.send(result, msg_type::method_result);
				break;
			}
			default:
				std::cerr << "error: unsuppurted distributed method ID\n";
			}
		}
		catch  (std::out_of_range&)
		{
			std::cerr << "error: object ID doesn't exist\n";
		}
	}

	DWORD get_ID()const { return m_client_ID; }
	std::map<DWORD, std::unique_ptr<distributed>> objects;
private:
	DWORD m_client_ID;	//ID (number) of the client
	static std::atomic<DWORD> client_number; ///< global static counter of clients
};

std::atomic<DWORD> CRemoteClient::client_number(1); ///< counter of clients. Begins from 1 to distinguish from uninitialized value 0.


///background thread routine to work with a pipe
void PipeCommunicate(CNamedPipeServer &&pipe)
{
	auto client = std::make_unique<CRemoteClient>();
	std::cout << "client #"<<client->get_ID()<<" is connected\n";
	auto buf = std::make_unique<message_t>();
	while(!flag_shutdown)
	{
		if(DWORD received_size = pipe.receive(buf.get(), sizeof(*buf)))
		{
			if (!flag_shutdown && received_size >= sizeof(msg_type))
			{
				switch (buf->type)
				{
				case	msg_type::protocol_version_request:
				{
					std::cout << "client #" << client->get_ID() << " asks protocol version\n";
					pipe.send(Protocol_version, msg_type::protocol_version);
					break;
				}
				case	msg_type::protocol_version:
				{
					auto ver = buf->data;
					if (ver != Protocol_version)
						std::cout << "client #" << client->get_ID() << " error! protocol version of the client differs from mine\n";
					break;
				}

				case	msg_type::description_request:
				{
					std::cout << "client #" << client->get_ID() << " asks description\n";
					pipe.send_container(std::string_view{ "I'm a pipe server!" }, msg_type::description);
					break;
				}
				case	msg_type::description:
				{
					std::string_view s{ buf->raw_data, buf->data };
					std::cout << "client #" << client->get_ID() << " sent it's description: " << s << '\n';
					break;
				}
				case	msg_type::clientID_request: pipe.send(client->get_ID(), msg_type::clientID); break;
				case	msg_type::string_msg:					
				{
					std::string_view s{ buf->raw_data, buf->data };
					std::cout << "client #" << client->get_ID() << " sent string: " << s << '\n';
					break;
				}

				case	msg_type::blob:							
				case	msg_type::create_object:				
				{
					auto result = client->create(distributed_classes( buf->data ));
					pipe.send(result, msg_type::created_object);
					break;
				}
				case	msg_type::delete_object:						client->destroy(int(buf->data)); break;
				case	msg_type::call_method:
				{
					message_rpc_t const& msg = *reinterpret_cast<message_rpc_t const *>(buf.get());
					client->execute(DWORD(buf->data), msg.method_ID, &msg.param, pipe);
					break;
				}
				default:
					std::cerr << "client #" << client->get_ID() << " sent unsupported message\n";
				}
			}
		}
		else
		{
			std::cout << "client #" << client->get_ID() << " is disconnected. Waiting for another...\n";
			pipe.disconnect();
			client.reset(); //releasing the client
			std::atomic_fetch_add(&waiting_pipes, 1);  // increasing number of waiting pipes (for not creating another thread)
			if(!flag_shutdown && pipe.wait_connection()) // waiting for another client
			{
				std::atomic_fetch_sub(&waiting_pipes, 1); // decreasing number of waiting pipes.
				client = std::make_unique<CRemoteClient>(); // creating a client for the pipe
				std::cout << "client #" << client->get_ID() << " is connected\n";
			}
		}
	}
}

/// a class for keeping all threads and closing them on program exit.
class Cthreads_keeper : public std::vector<std::thread>
{
public:
	~Cthreads_keeper() { wait4all(); }
	void wait4all() //waits until all thread are closed
	{
		for (auto t = begin(); t != end(); ++t)
			if(t->joinable())
				t->join();
		clear();
		std::cout << "all threads are now closed\n";
	};

};
Cthreads_keeper threads_keeper;	//static instance of the class

/// a routine for handling Ctrl-C and Ctrl-F4 keyboard shortcuts along with closing the console window.
BOOL WINAPI HandlerRoutine(_In_ DWORD /*dwCtrlType*/)
{
	flag_shutdown = true;
	// most of the threads are usually waiting for connection. So, connect for wakening them.
	while (true)
	{
		CNamedPipeClient pipe; 
		if (!pipe.open()) // until there is no created pipes.
			break;
	}
	
	threads_keeper.wait4all(); //waiting for closing all threads 
	return true;//now we can close the application
}
																
int main()
{
    std::cout << "I'm a server!\nPress Ctrl-C or Ctrl-F4 (or use your mouse) for closing the server\n";
	SetConsoleCtrlHandler(&HandlerRoutine, true); //handling Ctrl-C and Ctrl-F4
	threads_keeper.reserve(max_connections);

	while (!flag_shutdown) //until flagged for shutting down
	{
		if(waiting_pipes == 0) //creating new pipe only if there is no any free pipe
		{
			if (threads_keeper.size() < max_connections) // and only if the number of connections less then max
			{
				CNamedPipeServer pipe;
				if (pipe.create())
				{
					std::atomic_fetch_add(&waiting_pipes, 1); // one more available pipe
					if (pipe.wait_connection())
					{
						std::atomic_fetch_sub(&waiting_pipes, 1); // the pipe is now busy
						std::thread t(PipeCommunicate, std::move(pipe)); //move the pipe to the worker thread
						if (t.joinable())
							threads_keeper.push_back(std::move(t)); //add the thread to the trhead keeper
					}
				}
			}
			else
				std::cout << "maximum alowed connections reached\n";
		}
		Sleep(1000); //just wait a little
	}

	std::cout << "closing the server\n";
}
