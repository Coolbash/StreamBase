#include "distributed.h"
#include <atomic>
#include <iostream>

//---------------------------------------------------------------
std::atomic<DWORD> distributed_counter(1);  ///< global static counter of objecs. Needed for creating unique object ID. Protected for multithreaded using.
//---------------------------------------------------------------
distributed::distributed(DWORD client): m_client_ID{ client } 
{
	m_ID = std::atomic_fetch_add(&distributed_counter, 1);
};
//---------------------------------------------------------------

std::string_view distributed::get_class_name()
{
	return "distrubuted";
}
//---------------------------------------------------------------

distributed_int::distributed_int(int client_num) :distributed{client_num}
{
	std::cout << "created instance #" << get_ID() << " of distributed_int for client #" << client_num << '\n';
}
//---------------------------------------------------------------

distributed_int::~distributed_int()
{
	std::cout << "distributed_int instance #" << get_ID() << " is deleted\n";
}
//---------------------------------------------------------------

std::string_view distributed_int::get_class_name()
{
	return "distributed_int";
}
//---------------------------------------------------------------

bool distributed_int::set_param(simple_variant_t param)
{
	try
	{
		m_client_param = std::get<long long>(param);
		return true;
	}
	catch (std::bad_variant_access&)
	{
		std::cerr << "error: wrong variant data\n";
	}
	return false;
};
//---------------------------------------------------------------

simple_variant_t distributed_int::get_param()
{
	return m_client_param;
}
//---------------------------------------------------------------

distributed_double::distributed_double(int client_num) :distributed{ client_num }
{
	std::cout << "created instance #" << get_ID() << " of distributed_double for client #" << client_num << '\n';
}
//---------------------------------------------------------------

distributed_double::~distributed_double()
{
	std::cout << "distributed_double instance #" << get_ID() << " is deleted\n";
}
//---------------------------------------------------------------

std::string_view distributed_double::get_class_name()
{
	return "distributed_double";

}
//---------------------------------------------------------------

bool distributed_double::set_param(simple_variant_t param)
{
	try
	{
		m_client_param = std::get<double>(param);
		return true;
	}
	catch (std::bad_variant_access&)
	{
		std::cerr << "error: wrong variant data\n";
	}
	return false;
};
//---------------------------------------------------------------

simple_variant_t distributed_double::get_param()
{
	return m_client_param;
}
//---------------------------------------------------------------
//---------------------------------------------------------------




