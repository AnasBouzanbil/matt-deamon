#ifndef WEB_INTERFACE_HPP
#define WEB_INTERFACE_HPP

#include <string>

class WebInterface {
public:
    // Main page generation functions
    static std::string get_login_page();
    static std::string get_dashboard_page();
    
    // HTTP response helpers
    static std::string create_http_response(const std::string& content, const std::string& content_type = "text/html");
    static std::string create_json_response(const std::string& json_content);
    static std::string create_redirect_response(const std::string& location);
    
private:
    // HTML template components
    static std::string get_common_styles();
    static std::string get_login_styles();
    static std::string get_dashboard_styles();
    static std::string get_common_scripts();
    static std::string get_dashboard_scripts();
};

#endif // WEB_INTERFACE_HPP
