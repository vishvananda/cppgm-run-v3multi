#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "exceptions.h"
#include "macro.h"
#include "posttoken.h"

namespace
{

char hex_digit(unsigned int value)
{
	return value < 10 ? static_cast<char>('0' + value) :
		static_cast<char>('A' + value - 10);
}

std::string hex_dump(const std::vector<std::uint8_t>& bytes)
{
	std::string result;
	result.reserve(bytes.size() * 2);
	for (std::size_t i = 0; i < bytes.size(); ++i)
	{
		result.push_back(hex_digit(bytes[i] >> 4));
		result.push_back(hex_digit(bytes[i] & 0x0F));
	}
	return result;
}

struct DebugPostTokenOutput : IPostTokenOutput
{
	void emit_invalid(const std::string& source)
	{
		std::cout << "invalid " << source << '\n';
	}

	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		std::cout << "simple " << source << " "
			<< simple_token_type_name(type) << '\n';
	}

	void emit_identifier(const std::string& source)
	{
		std::cout << "identifier " << source << '\n';
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		std::cout << "literal " << source << " ";
		if (value.element_count == 0)
			std::cout << fundamental_type_name(value.type) << " ";
		else
			std::cout << "array of " << value.element_count << " "
				<< fundamental_type_name(value.type) << " ";
		std::cout << hex_dump(value.bytes) << '\n';
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		std::cout << "user-defined-literal " << value.source << " "
			<< value.suffix << " ";
		switch (value.kind)
		{
		case UserDefinedLiteralKind::Integer:
			std::cout << "integer " << value.prefix << '\n';
			break;
		case UserDefinedLiteralKind::Floating:
			std::cout << "floating " << value.prefix << '\n';
			break;
		case UserDefinedLiteralKind::Character:
			std::cout << "character "
				<< fundamental_type_name(value.value.type) << " "
				<< hex_dump(value.value.bytes) << '\n';
			break;
		case UserDefinedLiteralKind::String:
			std::cout << "string array of " << value.value.element_count << " "
				<< fundamental_type_name(value.value.type) << " "
				<< hex_dump(value.value.bytes) << '\n';
			break;
		}
	}

	void emit_eof()
	{
		std::cout << "eof\n";
	}
};

} // namespace

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	try
	{
		std::ostringstream source;
		source << std::cin.rdbuf();
		PPTokenBuffer tokens;
		preprocess_cpp_source(source.str(), &tokens);
		DebugPostTokenOutput output;
		posttokenize_cpp_tokens(tokens, output);
		return EXIT_SUCCESS;
	}
	catch (const NotImplementedException& e)
	{
		std::cerr << "ERROR: " << e.what() << '\n';
		return CPPGM_EXIT_NOT_IMPLEMENTED;
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << '\n';
		return EXIT_FAILURE;
	}
}
