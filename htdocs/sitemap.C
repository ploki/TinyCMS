#include<raii.H>
using namespace raii;

class SERVLET(sitemap) : public HttpServlet {
	void service(HttpServletRequest& request, HttpServletResponse& response) {
		response.setContentType("text/xml");
		request.getRequestDispatcher("/view/sitemap.csp").forward(request,response);
	}
};
exportServlet(sitemap);
