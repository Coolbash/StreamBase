#include "client.h"
#include <string>

//---------------------------------------------------------------
std::string_view CClient::m_description = "StreamBase single-threaded named-pipe client.";
//---------------------------------------------------------------

bool CClient::dispatch_received_data()
{
	switch (m_buf.type)
	{
	case	msg_type::protocol_version_request: m_pipe.send(Protocol_version, msg_type::protocol_version); break;
	case	msg_type::protocol_version:
	{
		m_version_ok = m_buf.data == Protocol_version;
		if (m_version_ok)
			std::cout << "protocol versions corresponds\n";
		else
			std::cout << "error! protocol version of the server differs from mine\n";
		break;
	}

	case	msg_type::description_request:
		m_pipe.send_container(m_description, msg_type::description); 
		break;

	case	msg_type::description:
	{
		m_server_description.assign(m_buf.raw_data, m_buf.data);
		std::cout << "server description: " << m_server_description << '\n';
		break;
	}

	case	msg_type::clientID:
	{
		m_ID = m_buf.data;
		std::cout << "this client ID: " << m_ID << '\n';
		break;
	}

	case	msg_type::string_msg:
	{
		std::string_view s{ m_buf.raw_data, m_buf.data };
		std::cout << "server sent string: " << s << '\n';
		break;
	}

	default:
		std::cerr << "received unsupported data message the server\n";
	}

	return true;
}
//---------------------------------------------------------------

bool CClient::receive_and_dispatch()
{
	if (auto received_size = m_pipe.receive(&m_buf, sizeof(m_buf)))
		return dispatch_received_data();
	return false;
}
//---------------------------------------------------------------

template<typename T> std::optional<T>CClient::receive_simple_data()
{
	if (auto received_size = m_pipe.receive(&m_buf, sizeof(m_buf)))
	{
		if (received_size >= sizeof(T) + sizeof(msg_type))
			return reinterpret_cast<simple_t<T>const*>(&m_buf)->data;
	}
	return std::nullopt;
}
//---------------------------------------------------------------

std::optional<std::string_view>  CClient::recieve_string()
{
	if (auto received_size = m_pipe.receive(&m_buf, sizeof(m_buf)))
	{
		if (received_size > 8)
			return std::string_view{ m_buf.raw_data, m_buf.data };
	}
	return std::nullopt;
}
//---------------------------------------------------------------

bool CClient::communicate_with_object(DWORD id)
{
	message_rpc_t msg{ msg_type::call_method, id, distributed_methods::get_class_name, 0};
	if (!m_pipe.send(&msg, sizeof(msg))) return false;
	//requesting object's clss name
	if (auto class_name = recieve_string())
	{
		auto s = class_name.value();
		std::cout << "created an object with class name " << s << '\n';
	}

	//checking object's ID
	msg.method_ID = distributed_methods::get_instance;
	if (!m_pipe.send(&msg, sizeof(msg))) return false;
	if (auto tmp_id = receive_simple_data<DWORD>())
	{
		if (tmp_id.value() == id)
			std::cout << "remote object object ID corresponds\n";
		else
			std::cerr << "error: remote object returned different ID\n";
	}

	//chekcing client's ID
	msg.method_ID = distributed_methods::get_client;
	if (!m_pipe.send(&msg, sizeof(msg))) return false;
	if (auto tmp_id = receive_simple_data<DWORD>())
	{
		if (tmp_id.value() == m_ID)
			std::cout << "remote object client ID corresponds\n";
		else
			std::cerr << "error: remote object returned different client ID\n";
	}

	return true;
}
//---------------------------------------------------------------

bool CClient::communicate_with_object_int(DWORD id)
{
	if (communicate_with_object(id))
	{
		constexpr long long param = 42;	// just some number for checking
		message_rpc_t msg{ msg_type::call_method, id, distributed_methods::set_param, param}; //structure for sending
		if (!m_pipe.send(&msg, sizeof(msg))) return false; // calling the method

		msg.method_ID = distributed_methods::get_param;
		if (!m_pipe.send(&msg, sizeof(msg))) return false;
		if (auto tmp_id = receive_simple_data<simple_variant_t>())
		{
			try
			{
				auto p = tmp_id.value();
				if (std::get<long long>(p) == param)
					return true;
			}catch (std::bad_variant_access&) 
			{
				std::cerr << "error: remote object sent wrong wariant data\n";
			}
			std::cerr << "error: remote object returned different param\n";
		}
	}
	return false;
}
//---------------------------------------------------------------

bool CClient::communicate_with_object_double(DWORD id)
{
	auto IsEqual = [](double x, double y)	//a lambla for compare doubles
	{
		return std::fabs(x - y) <= std::numeric_limits<double>::epsilon();
	};

	if (communicate_with_object(id))
	{
		constexpr double param = 3.14159; // just some param for checking

		message_rpc_t msg{ msg_type::call_method, id, distributed_methods::set_param, param };
		if (!m_pipe.send(&msg, sizeof(msg))) return false;

		msg.method_ID = distributed_methods::get_param;
		if (!m_pipe.send(&msg, sizeof(msg))) return false;
		if (auto tmp_id = receive_simple_data<simple_variant_t>())
		{
			try
			{
				if ( IsEqual(std::get<double>(tmp_id.value()), param) )
					return true;
			}
			catch (std::bad_variant_access&)
			{
				std::cerr << "error: remote object sent wrong wariant data\n";
			}
			std::cerr << "error: remote object returned different param\n";
		}
	}

	return false;
}
//---------------------------------------------------------------


bool CClient::communicate()
{
	if (m_pipe.send(msg_type::protocol_version_request) && receive_and_dispatch() && m_version_ok) //first thing making one transaction and check protocol version
	{
		//asynchronosly call other requests
		if (m_pipe.send(msg_type::description_request)
			&& m_pipe.send_container(m_description, msg_type::description)
			&& m_pipe.send(msg_type::clientID_request)
			)
		{
			Sleep(500); //just 
			while (m_pipe.bytesForRead())
				receive_and_dispatch();
			//trying to create an object 
			if (m_pipe.send(distributed_classes::distributed_int, msg_type::create_object))
			{
				if (auto id = receive_simple_data<DWORD>())
				{
					communicate_with_object_int(id.value());
					m_pipe.send(id.value(), msg_type::delete_object);
				}
			}

			//trying to create many objects and "forgetting" to delete them
			bool result = true;
			for (int i = 10; i && result; --i)
			{
				if (m_pipe.send(distributed_classes::distributed_double, msg_type::create_object))
				{
					if (auto id = receive_simple_data<DWORD>())
						communicate_with_object_double(id.value());
				}
			}

			std::cout << "enter a message for the server\n";
			std::string line;
			std::getline(std::cin, line);
			m_pipe.send_container(line, msg_type::string_msg);

			std::cout << "It all looks good. Exiting...\n";
			return true;
		}
	}
	return false;
}
//---------------------------------------------------------------
//---------------------------------------------------------------
