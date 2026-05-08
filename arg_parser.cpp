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

int parse_int_in_range(const string &value,
                       int min_value,
                       int max_value,
                       const string &flag_name)
{
    int parsed;

    try
    {
        size_t pos = 0;
        parsed = stoi(value, &pos);

        if (pos != value.size())
        {
            throw invalid_argument("not whole number");
        }
    }
    catch (const exception &)
    {
        throw invalid_argument(
            "Invalid value for " + flag_name + ": " + value);
    }

    if (parsed < min_value || parsed > max_value)
    {
        throw invalid_argument(
            "Value for " + flag_name + " out of range: " + value);
    }

    return parsed;
}

Options parse_args(int argc, char *argv[])
{
    Options opt = {};

    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];

        if (arg.empty() || arg[0] != '-')
        {
            throw invalid_argument("Invalid argument: " + arg);
        }
        // 1 to omit - before flag in string
        for (size_t j = 1; j < arg.length(); j++)
        {
            switch (arg[j])
            {
            case 'u':
            {
                opt.url = get_option_value(arg, j, i, argc, argv, 'u');

                if (opt.url.empty())
                {
                    throw invalid_argument("Empty URL for -u");
                }

                j = arg.size();
                break;
            }
            case 'm':
                opt.m = true;
                break;
            case 't':
            {
                string value = get_option_value(arg, j, i, argc, argv, 't');
                opt.timeout = parse_int_in_range(value, 100, 100000, "-t");
                j = arg.size();
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
                opt.verbosity = parse_int_in_range(value, 0, 4, "-v");
                j = arg.size();
                break;
            }
            case 'q':
                opt.verbosity = 0;
                break;
            default:
                throw invalid_argument("Unknown option: " + arg[j]);
            }
        }
    }
    
    if (opt.url.empty())
    {
        throw invalid_argument("Missing required -u option");
    }

    return opt;
}