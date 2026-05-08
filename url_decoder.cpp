#include "common.h"

static bool is_digits_only(const std::string &s)
{
    if (s.empty())
    {
        return false;
    }

    for (unsigned char c : s)
    {
        if (!std::isdigit(c))
        {
            return false;
        }
    }

    return true;
}

ParsedUrl parse_url(const string &url)
{
    ParsedUrl result;

    if (url.empty())
    {
        throw std::invalid_argument("URL is empty");
    }

    if (url.find(' ') != std::string::npos)
    {
        throw std::invalid_argument("URL contains spaces");
    }

    string rest = "";

    // Check for http:// or https:// prefix
    if (url.rfind("http://", 0) == 0)
    {
        result.use_tls = false;
        rest = url.substr(7);
    }
    else if (url.rfind("https://", 0) == 0)
    {
        result.use_tls = true;
        rest = url.substr(8);
    }
    else
    {
        throw std::invalid_argument("Only http:// and https:// URLs are supported");
    }

    if (rest.empty())
    {
        throw std::invalid_argument("Missing host");
    }

    // Split authority and path
    size_t slash_pos = rest.find('/');
    // Authority = host:port
    string authority = (slash_pos == std::string::npos) ? rest : rest.substr(0, slash_pos);
    string path_and_query = (slash_pos == std::string::npos) ? "/" : rest.substr(slash_pos);

    if (authority.empty())
    {
        throw std::invalid_argument("Missing host");
    }

    // Handle IPv6 addresses enclosed in [] and optional port
    if (authority[0] == '[')
    {
        size_t closing_bracket = authority.find(']');

        if (closing_bracket == std::string::npos)
        {
            throw std::invalid_argument("Missing closing bracket in IPv6 address");
        }

        result.host = authority.substr(1, closing_bracket - 1);

        if (result.host.empty())
        {
            throw std::invalid_argument("Missing host");
        }

        // Check for optional port after IPv6 address
        if (closing_bracket + 1 == authority.size())
        {
            result.port = result.use_tls ? "443" : "80";
        }
        else
        {
            if (authority[closing_bracket + 1] != ':')
            {
                throw std::invalid_argument("Invalid IPv6 host format");
            }

            result.port = authority.substr(closing_bracket + 2);

            if (!is_digits_only(result.port))
            {
                throw std::invalid_argument("Port must be numeric");
            }
        }
    }
    else
    {
        // Handle regular host:port format
        size_t colon_pos = authority.rfind(':');

        if (colon_pos == std::string::npos)
        {
            result.host = authority;
            result.port = result.use_tls ? "443" : "80";
        }
        else
        {
            result.host = authority.substr(0, colon_pos);
            result.port = authority.substr(colon_pos + 1);

            if (result.host.empty())
            {
                throw std::invalid_argument("Missing host");
            }

            if (!is_digits_only(result.port))
            {
                throw std::invalid_argument("Port must be numeric");
            }
        }
    }

    // Validate path and query
    if (path_and_query.empty())
    {
        result.target = "/";
    }
    else
    {
        if (path_and_query[0] != '/')
        {
            throw std::invalid_argument("Invalid target");
        }
        result.target = path_and_query;
    }

    // Validate port number range
    int port_num = std::stoi(result.port);
    if (port_num < 1 || port_num > 65535)
    {
        throw std::invalid_argument("Port out of range");
    }

    return result;
}