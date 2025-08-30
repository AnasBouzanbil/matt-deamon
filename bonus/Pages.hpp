#ifndef PAGES_HPP
#define PAGES_HPP
#include <string>

std::string render_success_page(const std::string& username, int client_id, const std::string& log_content);
std::string render_failure_page();

#endif // PAGES_HPP
