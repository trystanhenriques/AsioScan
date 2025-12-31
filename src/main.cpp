#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        std::cout << "AsioScan: Boost.Asio smoke test starting...\n";

        boost::asio::io_context io;

        boost::asio::steady_timer timer(io);
        timer.expires_after(std::chrono::seconds(1));

        timer.async_wait([](const boost::system::error_code& ec) {
            if (!ec) {
                std::cout << "Timer fired successfully.\n";
            } else {
                std::cerr << "Timer error: " << ec.message() << "\n";
            }
        });

        io.run();

        std::cout << "AsioScan: Boost.Asio smoke test completed.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
