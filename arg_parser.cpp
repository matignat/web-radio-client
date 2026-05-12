/*
 * Command-line argument parsing utilities for the radio client.
 *
 * Handles option values, validates numeric parameters, and builds the
 * configuration used to start the client according to program arguments.
 */

#include "common.h"

string get_option_value(const string &arg, size_t j,
                        int &i, int argc, char *argv[], char flag)
{
    if (j + 1 < arg.size())
    {
        return arg.substr(j + 1);
    }

    if (i + 1 >= argc)
    {
        throw invalid_argument(
            string("Missing value for -") + flag);
    }

    return argv[++i];
}

// Parses integer from string and checks if it's in the specified range.
int parse_int_in_range(const string &value, int min_value, int max_value, const string &flag_name)
{
    int parsed;
    size_t pos = 0;

    try
    {
        parsed = stoi(value, &pos);
    }
    catch (const exception &)
    {
        throw invalid_argument("Invalid value for " + flag_name + ": " + value);
    }

    if (pos != value.size())
    {
        throw invalid_argument("Invalid value for " + flag_name + ": " + value);
    }

    if (parsed < min_value || parsed > max_value)
    {
        throw invalid_argument("Value for " + flag_name + " out of range: " + value);
    }

    return parsed;
}

// Parses command-line arguments and returns an Options struct,
// In case of duplicate arguments the last one takes precedence.
Options parse_args(int argc, char *argv[])
{
    Options opt;

    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];

        if (arg.empty() || arg[0] != '-')
        {
            throw invalid_argument("Invalid argument: " + arg);
        }

        bool consumed = false;
        for (size_t j = 1; j < arg.length() && !consumed; j++)
        {
            switch (arg[j])
            {
            case 'u':
            {
                opt.url = get_option_value(arg, j, i, argc, argv, 'u');
                if (opt.url.empty())
                    throw invalid_argument("Empty URL for -u");
                consumed = true;
                break;
            }
            case 'm':
                opt.m = true;
                break;
            case 't':
            {
                string value = get_option_value(arg, j, i, argc, argv, 't');
                opt.timeout = parse_int_in_range(value, MIN_TIMEOUT_MS, MAX_TIMEOUT_MS, "-t");
                consumed = true;
                break;
            }
            case '4':
                opt.ip4 = true;
                break;
            case '6':
                opt.ip6 = true;
                break;
            case 'v':
            {
                string value = get_option_value(arg, j, i, argc, argv, 'v');
                opt.verbosity = parse_int_in_range(value, MIN_VERBOSITY, MAX_VERBOSITY, "-v");
                consumed = true;
                break;
            }
            case 'q':
                opt.verbosity = 0;
                break;
            default:
                throw invalid_argument(string("Unknown option: -") + arg[j]);
            }
        }
    }

    if (opt.url.empty())
    {
        throw invalid_argument("Missing required -u option");
    }

    return opt;
}