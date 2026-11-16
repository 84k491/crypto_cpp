#include "RemoteConnection.h"

#include <istream>
#include <tuple>

RemoteConnection::RemoteConnection(boost::asio::ip::tcp::socket socket)
    : m_socket(std::move(socket))
{
}

void RemoteConnection::send_line(const std::string & line)
{
    std::lock_guard lock(m_write_mutex);
    boost::system::error_code error;
    boost::asio::write(m_socket, boost::asio::buffer(line + '\n'), error);
}

std::optional<std::string> RemoteConnection::read_line()
{
    boost::system::error_code error;
    boost::asio::read_until(m_socket, m_read_buffer, '\n', error);
    if (error) {
        return std::nullopt;
    }

    std::istream stream(&m_read_buffer);
    std::string line;
    std::getline(stream, line);
    return line;
}

void RemoteConnection::shutdown()
{
    boost::system::error_code error;
    std::ignore = m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
}
