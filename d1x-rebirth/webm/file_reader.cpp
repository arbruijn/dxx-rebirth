#include "file_reader.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstring>

namespace uvpx
{
    FileReader::FileReader() :
        m_length(0),
        m_file(nullptr),
        m_preloaded(false),
        m_data(nullptr),
        m_do_close(false)
    {
    }

    FileReader::~FileReader()
    {
        close();
    }

    int FileReader::open(const char *fileName, bool preload)
    {
    	SDL_RWops *file = SDL_RWFromFile(fileName, "rb");
    	if (!file)
    		return -1;
    	int ret = open(file, preload);
    	if (ret)
    		SDL_RWclose(file);
		else
    		m_do_close = true;
		return ret;
    }

    int FileReader::open(SDL_RWops *file, bool preload)
    {
        if (file == nullptr)
            return -1;

        if (m_file)
            return -1;

        m_file = file;

        bool ret = !getFileSize();

        if (preload)
        {
            m_data = new unsigned char[m_length];
            Read(0, m_length, m_data);
        }

        m_preloaded = preload;

        return ret;
    }

    void FileReader::close()
    {
        SafeDeleteArray<unsigned char>(m_data);

        if (m_file != nullptr)
        {
        	if (m_do_close)
                SDL_RWclose(m_file);
            m_file = nullptr;
        }
    }

    int FileReader::Read(long long offset, long len, unsigned char* buffer)
    {
        if (m_file == nullptr)
            return -1;

        if (offset < 0)
            return -1;

        if (len < 0)
            return -1;

        if (len == 0)
            return 0;

        if (offset >= m_length)
            return -1;

        if (!m_preloaded)
        {
            SDL_RWseek(m_file, offset, RW_SEEK_SET);

            const size_t size = SDL_RWread(m_file, buffer, 1, len);

            if (size < size_t(len))
                return -1;  // error
        }
        // file preloaded
        else
        {
            size_t size = len;

            if ((offset + len) > m_length)
                size = m_length - offset;

            memcpy(buffer, m_data + offset, size);

            if (size < size_t(len))
                return -1;  // error
        }

        return 0;  // success
    }

    int FileReader::Length(long long* total, long long* available)
    {
        if (m_file == nullptr)
            return -1;

        if (total)
            *total = m_length;

        if (available)
            *available = m_length;

        return 0;
    }

    bool  FileReader::getFileSize()
    {
        if (m_file == NULL)
            return false;
        if (SDL_RWseek(m_file, 0L, RW_SEEK_END) == -1)
            return false;
        m_length = SDL_RWtell(m_file);

        if (m_length < 0)
            return false;

        if (SDL_RWseek(m_file, 0L, RW_SEEK_SET) == -1)
		    return false;

        return true;
    }
}
