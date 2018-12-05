// Copyright 2017 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_BSON_BSON_READER_HPP
#define JSONCONS_BSON_BSON_READER_HPP

#include <string>
#include <sstream>
#include <vector>
#include <istream>
#include <cstdlib>
#include <memory>
#include <limits>
#include <cassert>
#include <iterator>
#include <jsoncons/json.hpp>
#include <jsoncons/detail/source.hpp>
#include <jsoncons/json_content_handler.hpp>
#include <jsoncons/config/binary_detail.hpp>
#include <jsoncons_ext/bson/bson_detail.hpp>
#include <jsoncons_ext/bson/bson_error.hpp>

namespace jsoncons { namespace bson {

template <class Source>
class basic_bson_reader : public serializing_context
{
    Source source_;
    json_content_handler& handler_;
    size_t column_;
    size_t nesting_depth_;
public:
    basic_bson_reader(Source&& source, json_content_handler& handler)
       : source_(std::move(source)),
         handler_(handler), 
         column_(1),
         nesting_depth_(0)
    {
    }

    void read(std::error_code& ec)
    {
        uint8_t buf[sizeof(int32_t)]; 
        if (source_.read(buf, sizeof(int32_t)) != sizeof(int32_t))
        {
            JSONCONS_THROW(bson_error(0));
        }
        const uint8_t* endp;
        /* auto len = */jsoncons::detail::from_little_endian<int32_t>(buf, buf+sizeof(int32_t),&endp);

        handler_.begin_object(semantic_tag_type::none, *this);
        ++nesting_depth_;
        parse_e_list(bson_structure_type::document, ec);
        handler_.end_object(*this);
        --nesting_depth_;
    }

    void parse_e_list(bson_structure_type type, std::error_code& ec)
    {
        uint8_t t;
        while (source_.get(t) > 0 && t != 0x00)
        {
            std::basic_string<char> s;
            uint8_t c;
            while (source_.get(c) > 0 && c != 0)
            {
                s.push_back(c);
            }

            if (type == bson_structure_type::document)
            {
                handler_.name(basic_string_view<char>(s.data(),s.length()), *this);
            }
            parse_some(t, ec);
        }
    }

    void parse_some(uint8_t type, std::error_code& ec)
    {
        switch (type)
        {
            case bson_format::double_cd:
            {
                uint8_t buf[sizeof(double)]; 
                if (source_.read(buf, sizeof(double)) != sizeof(double))
                {
                    JSONCONS_THROW(bson_error(0));
                }
                const uint8_t* endp;
                double res = jsoncons::detail::from_little_endian<double>(buf,buf+sizeof(buf),&endp);
                handler_.double_value(res, floating_point_options(), semantic_tag_type::none, *this);
                break;
            }
            case bson_format::string_cd:
            {
                uint8_t buf[sizeof(int32_t)]; 
                if (source_.read(buf, sizeof(int32_t)) != sizeof(int32_t))
                {
                    JSONCONS_THROW(bson_error(0));
                }
                const uint8_t* endp;
                auto len = jsoncons::detail::from_little_endian<int32_t>(buf, buf+sizeof(buf),&endp);

                std::basic_string<char> s;
                s.reserve(len - 1);
                if (source_.read(len-1,std::back_inserter(s)) != len-1)
                {
                    JSONCONS_THROW(bson_error(0));
                }
                uint8_t c;
                source_.get(c); // discard 0
                handler_.string_value(basic_string_view<char>(s.data(),s.length()), semantic_tag_type::none, *this);
                break;
            }
            case bson_format::document_cd: 
            {
                read(ec);
                if (ec)
                {
                    return;
                }
                break;
            }

            case bson_format::array_cd: 
            {
                uint8_t buf[sizeof(int32_t)]; 
                if (source_.read(buf, sizeof(int32_t)) != sizeof(int32_t))
                {
                    JSONCONS_THROW(bson_error(0));
                }
                const uint8_t* endp;
                /* auto len = */ jsoncons::detail::from_little_endian<int32_t>(buf, buf+sizeof(int32_t),&endp);

                handler_.begin_array(semantic_tag_type::none, *this);
                ++nesting_depth_;
                parse_e_list(bson_structure_type::document, ec);
                handler_.end_array(*this);
                --nesting_depth_;
                break;
            }
            case bson_format::null_cd: 
            {
                handler_.null_value(semantic_tag_type::none, *this);
                break;
            }
            case bson_format::bool_cd:
            {
                uint8_t val;
                if (source_.get(val) == 0)
                {
                    JSONCONS_THROW(bson_error(0));
                }
                handler_.bool_value(val != 0, semantic_tag_type::none, *this);
                break;
            }
            case bson_format::int32_cd: 
            {
                uint8_t buf[sizeof(int32_t)]; 
                if (source_.read(buf, sizeof(int32_t)) != sizeof(int32_t))
                {
                    JSONCONS_THROW(bson_error(0));
                }
                const uint8_t* endp;
                auto val = jsoncons::detail::from_little_endian<int32_t>(buf, buf+sizeof(int32_t),&endp);
                handler_.int64_value(val, semantic_tag_type::none, *this);
                break;
            }

            case bson_format::timestamp_cd: 
            {
                uint8_t buf[sizeof(uint64_t)]; 
                if (source_.read(buf, sizeof(uint64_t)) != sizeof(uint64_t))
                {
                    JSONCONS_THROW(bson_error(0));
                }
                const uint8_t* endp;
                auto val = jsoncons::detail::from_little_endian<uint64_t>(buf, buf+sizeof(uint64_t),&endp);
                handler_.uint64_value(val, semantic_tag_type::epoch_time, *this);
                break;
            }

            case bson_format::int64_cd: 
            {
                uint8_t buf[sizeof(int64_t)]; 
                if (source_.read(buf, sizeof(int64_t)) != sizeof(int64_t))
                {
                    JSONCONS_THROW(bson_error(0));
                }
                const uint8_t* endp;
                auto val = jsoncons::detail::from_little_endian<int64_t>(buf, buf+sizeof(int64_t),&endp);
                handler_.int64_value(val, semantic_tag_type::none, *this);
                break;
            }
            case bson_format::binary_cd: 
            {
                uint8_t buf[sizeof(int32_t)]; 
                if (source_.read(buf, sizeof(int32_t)) != sizeof(int32_t))
                {
                    JSONCONS_THROW(bson_error(0));
                }
                const uint8_t* endp;
                const auto len = jsoncons::detail::from_little_endian<int32_t>(buf, buf+sizeof(int32_t),&endp);

                std::vector<uint8_t> v(len, 0);
                if (source_.read(v.data(), v.size()) != v.size())
                {
                    JSONCONS_THROW(bson_error(0));
                }

                handler_.byte_string_value(byte_string_view(v.data(),v.size()), 
                                           byte_string_chars_format::none, 
                                           semantic_tag_type::none, 
                                           *this);
                break;
            }
        }
        
    }

    size_t line_number() const override
    {
        return 1;
    }

    size_t column_number() const override
    {
        return column_;
    }
private:
};

typedef basic_bson_reader<jsoncons::detail::buffer_source> bson_reader;

// decode_bson

template<class Json>
Json decode_bson(const std::vector<uint8_t>& v)
{
    jsoncons::json_decoder<Json> decoder;
    basic_bson_reader<jsoncons::detail::buffer_source> parser{ v, decoder };
    std::error_code ec;
    parser.read(ec);
    if (ec)
    {
        throw parse_error(ec,parser.line_number(),parser.column_number());
    }
    return decoder.get_result();
}

}}

#endif
