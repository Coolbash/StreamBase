#pragma once
//in this file there is a cople of distributed classes that might be created remotely on server
#include "../common/global_types.h"

/// abstract class
class distributed
{
public:
	distributed(DWORD client_ID);
	virtual ~distributed()=default;			///< destructor must be virtual for this class
	virtual std::string_view get_class_name()=0;
	virtual bool set_param(simple_variant_t param)=0;
	virtual simple_variant_t get_param() = 0;
	DWORD get_ID() const { return m_ID; }
	DWORD get_client_ID() const { return m_client_ID; }

private:
	DWORD m_client_ID; ///< just for reference - number of client that works with this object
	DWORD m_ID;	///< a number of the object for identifying different instances remotelly
};


class distributed_int : public distributed
{
public:
	distributed_int(int client_ID);
	~distributed_int() override;
	std::string_view get_class_name() override;
	bool set_param(simple_variant_t param) override;
	simple_variant_t get_param() override;

private:
	long long m_client_param = 0;///< some param that the client sent through set_param
};


class distributed_double : public distributed
{
public:
	distributed_double(int client_ID);
	~distributed_double() override;
	std::string_view get_class_name() override;
	bool set_param(simple_variant_t param) override;
	simple_variant_t get_param() override;

private:
	double m_client_param = 0; ///< some param that the client sent through set_param
};



