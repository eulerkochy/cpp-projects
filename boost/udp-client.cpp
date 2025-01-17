#include <iostream>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::udp;

int main()
{
  try
  {
    // Create IO service
    boost::asio::io_service io_service;

    // Create UDP socket
    udp::socket socket(io_service);
    socket.open(udp::v4());

    // Create endpoint for server (localhost:1111)
    udp::endpoint server_endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"),
        1111);

    // Send an empty message to trigger server response
    std::string message = "";
    socket.send_to(boost::asio::buffer(message), server_endpoint);

    // Prepare buffer for response
    std::array<char, 128> recv_buffer;
    udp::endpoint sender_endpoint;

    // Receive response
    size_t len = socket.receive_from(
        boost::asio::buffer(recv_buffer),
        sender_endpoint);

    // Print response
    std::cout.write(recv_buffer.data(), len);
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Exception: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}