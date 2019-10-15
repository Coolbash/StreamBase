#pragma once

//this file contains global types that used in server and client
#include <windows.h>
#include <array>
#include <variant>

/// a structure for transmitting / receiving parameters and results for remote objects
using simple_variant_t = std::variant<int, long long, double>;

///maximum message length. Need for buffers and checking.
DWORD constexpr max_message_length = 0xffff;

///the max number of simultaneously connections due to unlimited connection number is not a reasonable choice..
int constexpr	max_connections = 4;	

///the pipe name
constexpr LPCSTR PipeName = R"(\\.\pipe\StreamBase)";

///type of messages; it includes in every message 
enum class msg_type
{
	protocol_version_request,		///< nothing; waiting responce protocol_version
	protocol_version,				///< DWORD
	description_request,			///< nothing; waiting for a string 
	description,					///< the same as for string
	clientID_request,				///< nothing; waiting responce with client ID (number)
	clientID,						///< int with ID (number)

	//basic numbers
	string_msg,						///< DWORD(size_in_bytes) + string_data;
	blob,							///< DWORD(size_in_bytes) + blob_data;

	//rpc
	create_object,					///< int(object_tipe_number); waiting for responce: object_handle;
	created_object,					///< int:  handle (number) of created object
	delete_object,					///< int(object_handle).
	call_method,					///< int(objec_handle) + int(method_number) + parameters in order of call. strings and blobs in correspodance format (size + data); waiting for result.
	method_result					///< int(object_handle) + int(method_number) + result;
};

/// types of supported distributed classes
enum class distributed_classes
{
	distributed_int,
	distributed_double
};

/// methods that can be called remotely
enum class distributed_methods
{
	get_class_name,
	get_instance,
	get_client,
	set_param,
	get_param
};

/// protocol version must be incremented if there is any change in the protocol
int constexpr Protocol_version = 42;

//structs for sending:
template<typename T>struct simple_t { msg_type t; T data; };	//for simple data like int, double, etc
template<typename T>struct blob_t	{ msg_type t; DWORD size; T data;}; //for blob data like string and binary
template<typename T>struct method_t { msg_type t; int handle; simple_variant_t param;}; //for calling a method of remote objects and returning result

///working structure for receiving data from a pipe
struct message_t
{
	msg_type	type = msg_type::protocol_version_request;	///< the only field that exist always
	DWORD		data = 0;									///< DWORD data. Might be size of blob (in elements), ID, handle, etc
	char		raw_data[max_message_length] = { 0 };		///< data of string, blob, or other binary data any size less than max_message_length
};

//data strucure of messages for calling methods
struct message_rpc_t
{
	msg_type				type; 
	DWORD					object_ID; 
	distributed_methods		method_ID;
	simple_variant_t		param;		///< placeholder for method parameters.
};



