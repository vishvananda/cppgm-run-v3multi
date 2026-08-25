// Student-facing scaffold for the PA13 `lowir2cy86` binary.

#include "exceptions.h"
#include "lowir2cy86_backend.h"
#include "tool_help_text.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {

vector<string> collect_args(int argc, char ** argv)
{
  vector<string> args;
  for(int i = 1; i < argc; ++i) {
    args.push_back(argv[i]);
  }
  return args;
}

bool has_help_arg(const vector<string> & args)
{
  for(size_t i = 0; i < args.size(); ++i) {
    if(args[i] == "--help" || args[i] == "-h") {
      return true;
    }
  }
  return false;
}

bool has_batch_stdin_arg(const vector<string> & args)
{
  for(size_t i = 0; i < args.size(); ++i) {
    if(args[i] == "--batch-stdin") {
      return true;
    }
  }
  return false;
}

vector<string> split_tabs(const string & line)
{
  vector<string> fields;
  size_t begin = 0;
  for(size_t i = 0; i <= line.size(); ++i) {
    if(i != line.size() && line[i] != '\t') {
      continue;
    }
    fields.push_back(line.substr(begin, i - begin));
    begin = i + 1;
  }
  return fields;
}

int run_batch_stdin_mode()
{
  string line;
  while(getline(cin, line)) {
    vector<string> fields = split_tabs(line);
    int status = EXIT_FAILURE;
    try {
      // The ordinary text-test worker sends three fields:
      // output-file, input-file, and the source path.  The generic wrapped
      // worker sends the full five-field request, whose arguments begin at
      // field four.  Supporting both makes the standalone driver useful and
      // keeps it compatible with the repository test runner.
      if(fields.size() == 3) {
        lowir2cy86::compile(vector<string>(1, fields[2]), fields[0]);
        status = EXIT_SUCCESS;
      }
      else if(fields.size() >= 5) {
        vector<string> args(fields.begin() + 4, fields.end());
        if(args.size() < 3 || args[0] != "-o") {
          throw logic_error("invalid batch invocation");
        }
        vector<string> sources(args.begin() + 2, args.end());
        lowir2cy86::compile(sources, args[1]);
        status = EXIT_SUCCESS;
      }
      else {
        throw logic_error("invalid batch request");
      }
    }
    catch(const exception & e) {
      cerr << "ERROR: " << e.what() << endl;
    }
    cout << (status == EXIT_SUCCESS ? "EXIT_SUCCESS" : "EXIT_FAILURE")
         << endl;
  }
  return EXIT_SUCCESS;
}

void parse_output_invocation(const vector<string> & args,
                             string & outfile,
                             vector<string> & srcfiles)
{
  if(args.size() < 3 || args[0] != "-o") {
    throw logic_error("invalid usage");
  }

  outfile = args[1];
  srcfiles.assign(args.begin() + 2, args.end());
}

int run_lowir2cy86_mode(const vector<string> & args)
{
  if(has_batch_stdin_arg(args)) {
    return run_batch_stdin_mode();
  }

  if(has_help_arg(args)) {
    cout << lowir2cy86_help_text();
    return EXIT_SUCCESS;
  }

  string outfile;
  vector<string> srcfiles;
  parse_output_invocation(args, outfile, srcfiles);

  lowir2cy86::compile(srcfiles, outfile);
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char ** argv)
{
  try
  {
    return run_lowir2cy86_mode(collect_args(argc, argv));
  }
  catch(const NotImplementedException & e)
  {
    cerr << "ERROR: " << e.what() << endl;
    return CPPGM_EXIT_NOT_IMPLEMENTED;
  }
  catch(const exception & e)
  {
    cerr << "ERROR: " << e.what() << endl;
    return EXIT_FAILURE;
  }
}
