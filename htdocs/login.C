#include <raii.H>

using namespace raii;

class SERVLET(login) : public HttpServlet {

	void service(HttpServletRequest& request, HttpServletResponse& response) {
		Logger log("tinycms");
		log("user login: "+request.getRemoteUser());
		ptr<HttpSession> session = request.getSession();
		session->setUser(request.getRemoteUser());
		session->setMaxInactiveInterval(24*3600);
		response.sendRedirect(response.encodeRedirectURL(getInitParameter("Alias")+request.getPathInfo()));
	}
};

exportServlet(login);
