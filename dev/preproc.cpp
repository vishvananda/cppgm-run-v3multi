#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "exceptions.h"
#include "posttoken.h"
#include "preproc_session.h"

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

class PostTokenOutput : public IPostTokenOutput
{
public:
	explicit PostTokenOutput(std::ostream& output) : output_(output) {}

	void emit_invalid(const std::string& source)
	{
		throw std::runtime_error("invalid posttoken: " + source);
	}

	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		output_ << "simple " << source << " "
			<< simple_token_type_name(type) << '\n';
	}

	void emit_identifier(const std::string& source)
	{
		output_ << "identifier " << source << '\n';
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		output_ << "literal " << source << " ";
		if (value.element_count == 0)
			output_ << fundamental_type_name(value.type) << " ";
		else
			output_ << "array of " << value.element_count << " "
				<< fundamental_type_name(value.type) << " ";
		output_ << hex_dump(value.bytes) << '\n';
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		output_ << "user-defined-literal " << value.source << " "
			<< value.suffix << " ";
		switch (value.kind)
		{
		case UserDefinedLiteralKind::Integer:
			output_ << "integer " << value.prefix << '\n';
			break;
		case UserDefinedLiteralKind::Floating:
			output_ << "floating " << value.prefix << '\n';
			break;
		case UserDefinedLiteralKind::Character:
			output_ << "character "
				<< fundamental_type_name(value.value.type) << " "
				<< hex_dump(value.value.bytes) << '\n';
			break;
		case UserDefinedLiteralKind::String:
			output_ << "string array of " << value.value.element_count << " "
				<< fundamental_type_name(value.value.type) << " "
				<< hex_dump(value.value.bytes) << '\n';
			break;
		}
	}

	void emit_eof()
	{
		output_ << "eof\n";
	}

private:
	std::ostream& output_;
};

std::string read_source(const std::string& path)
{
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input)
		throw std::runtime_error("unable to open source file: " + path);
	std::ostringstream source;
	source << input.rdbuf();
	return source.str();
}

PPPreprocessConfig build_config()
{
	const std::time_t now = std::time(NULL);
	const std::tm* local = std::localtime(&now);
	const char* stamp = local == NULL ? NULL : std::asctime(local);
	if (stamp == NULL || std::string(stamp).size() < 24)
		throw std::runtime_error("unable to determine build date and time");
	const std::string asctime_value(stamp);
	const std::string date = asctime_value.substr(4, 7) +
		asctime_value.substr(20, 4);
	const std::string time = asctime_value.substr(11, 8);
	return PPPreprocessConfig("Vishvananda Abrams", date, time);
}

} // namespace

int main(int argc, char** argv)
{
	try
	{
		if (argc < 4 || std::string(argv[1]) != "-o")
			throw std::logic_error("invalid usage");
		const std::string outfile(argv[2]);
		const std::size_t source_count = static_cast<std::size_t>(argc - 3);
		std::ofstream output(outfile.c_str());
		if (!output)
			throw std::runtime_error("unable to open output file: " + outfile);

		const PPPreprocessConfig config = build_config();
		output << "preproc " << source_count << '\n';
		for (int i = 3; i < argc; ++i)
		{
			const std::string source_path(argv[i]);
			output << "sof " << source_path << '\n';
			const std::string source = read_source(source_path);
			PPPreprocessingSession session(config);
			const PPTokenBuffer& tokens = session.preprocess(source_path, source);
			PostTokenOutput token_output(output);
			posttokenize_cpp_tokens(tokens, token_output);
		}
		return EXIT_SUCCESS;
	}
	catch (const NotImplementedException& error)
	{
		std::cerr << "ERROR: " << error.what() << '\n';
		return CPPGM_EXIT_NOT_IMPLEMENTED;
	}
	catch (const std::exception& error)
	{
		std::cerr << "ERROR: " << error.what() << '\n';
		return EXIT_FAILURE;
	}
}
