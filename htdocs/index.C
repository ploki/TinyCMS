#include <raii.H>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace raii;
using namespace raii::sql;

class SERVLET(index) : public HttpServlet {

	void service(HttpServletRequest& request, HttpServletResponse& response) {

		//pathInfo stuffs
		String pathInfo=request.getPathInfo();
		bool mod_action=getInitParameter("Action")=="true";
		if ( mod_action )
			pathInfo=pathInfo.substr(getInitParameter("Alias").size(),String::npos);

		//session stuffs
		ptr<HttpSession> session=request.getSession();
		String tinycmsid= request.getParameter("TINYCMSID");

		if ( mod_action ) {
			ptr<ServletContext> sc = getServletContext();
			if ( !tinycmsid.empty() )
				session = sc->getSession(request.getParameter("TINYCMSID"));
			else
				session = NULL;
			if ( !session ) {
				session=request.getSession();
				Cookie c;
				c.setName("TINYCMSID");
				c.setValue(session->getId());
				c.setPath(getInitParameter("Alias"));
				response.addCookie(c);
			}
		}

		request.setAttribute("session",session);
		request.setAttribute("pathInfo",new String(pathInfo));

		if ( mod_action && pathInfo.substr(0,sizeof("/login")-1) == "/login" ) {
			session->setUser(request.getRemoteUser());
			Logger log("tinycms");
			log("user login: "+request.getRemoteUser());
			pathInfo=pathInfo.substr(sizeof("/login")-1,String::npos);
			response.sendRedirect(response.encodeRedirectURL(getInitParameter("Alias")+pathInfo));
		}


		Connection conn;
		conn.query("SELECT 1");
		try {
			conn.query("SELECT * FROM page WHERE 1=0");
		}
		catch (...) {
			conn.query("CREATE TABLE page ( pathinfo VARCHAR, title VARCHAR, body VARCHAR, creator VARCHAR, date TIMESTAMP)");
		}

		String user = request.getSession()->getUser();
		bool logged = !user.empty();

		if ( pathInfo.empty() ) {
			response.sendRedirect(response.encodeRedirectURL(getInitParameter("Alias")+"/"));
		}
		if ( pathInfo.size() != 1 
			&& pathInfo[pathInfo.size()-1] == '/' ) {
			response.sendRedirect(response.encodeRedirectURL(getInitParameter("Alias")+pathInfo.substr(0,pathInfo.size()-1)));
		}
		String action = request.getParameter("action");
		if ( action.empty() ) {
			request.getRequestDispatcher("/view/front.csp").forward(request,response);
		}
		else if ( action == "edit" ) {
			if ( logged )
				request.getRequestDispatcher("/view/edit.csp").forward(request,response);
			response.sendRedirect(response.encodeRedirectURL(getInitParameter("Alias")+pathInfo));
		}
		else if ( action == "update" ) {
			if ( logged  ) {
				if (request.getParameter("cancel").empty() )
					conn.query("INSERT INTO page VALUES('"+conn.sqlize(pathInfo)+"','"+conn.sqlize(request.getParameter("title"))+"','"+conn.sqlize(request.getParameter("body"))+"','"+conn.sqlize(user)+"','"+itostring(time(NULL))+"')");
			}
			else
				throw ServletException("login required");
			response.sendRedirect(response.encodeRedirectURL(getInitParameter("Alias")+pathInfo));
		}
		else if ( action == "external_link_list" ) {
			if ( logged ) {
				response.setContentType("text/javascript");
				response << "var tinyMCELinkList = new Array(";
				ResultSet rs=conn.query("SELECT pathinfo,title,date FROM page ORDER BY pathinfo,date");
				Map<String,String> path;
				while ( rs.next() ) {
					path[rs["pathinfo"]]=rs["title"];
				}
				for ( Map<String,String>::iterator it = path.begin() ; it != path.end() ; ) {
					response << "[ \""<<it->second<<"\", \""<<getInitParameter("Alias")+it->first<<"\"]";
					if ( ++it != path.end() )
						response << ", ";
				}
				{
				//ajout des éléments de IMG
				struct dirent **entry;
				char *_dir  = strdupa(apacheRequest->filename);
				while ( _dir[strlen(_dir)-1] != '/' && _dir[0] != '\0' ) _dir[strlen(_dir)-1] = '\0';
				String dir=_dir;
				int m = scandir((dir+"/IMG").c_str(),&entry,0,versionsort);
				bool first=path.empty()?true:false;
				for (int n = 0 ; n < m ; ++n ) {
					bool forcedir=false;
					String filename=String("IMG/")+entry[n]->d_name;
					if ( entry[n]->d_type & DT_LNK ) {
						struct stat st;
						if ( stat(String(dir+filename+String("/")).c_str(),&st) == 0  ) {
							if ( S_ISDIR(st.st_mode) )
                                                		forcedir=true;
						}
					}
					if ( entry[n]->d_name[0] == '.' || forcedir || entry[n]->d_type & DT_DIR ) {
						//nop
					}
					else {
						if ( !first ) {
							 response << ",";
						}
						response << "[ \"" << String(entry[n]->d_name) <<  "\",\""<<request.getContextPath()+"/"+filename<<"\"]";
						first = false;
					}
					free(entry[n]);
				}
				if ( m > 0 ) free(entry);

				}
				response << ");";
			}
		}
		else if ( action == "getlist" ) {
			if ( logged ) {
				response.setContentType("text/javascript");
				response << "var tinyMCETemplateList = [";
				setlocale(LC_ALL,"fr_FR.UTF-8");
				ResultSet rs=conn.query("SELECT date FROM page where pathinfo='"+conn.sqlize(pathInfo)+"' ORDER by date DESC");
				if ( rs.next() ) do {
					char buf[1024];
					time_t ts=rs["date"].toi();
					tm tmm;
					localtime_r(&ts,&tmm);
					strftime(buf,1024,"%A %d %B %Y à %H:%M:%S",&tmm);
					response << "[\"modification du " << String(buf) <<  "\",\""<<getInitParameter("Alias")+pathInfo<<"?action=getversion&date="<<rs["date"]<<"\"]";
					if ( rs.next() )
						response << ", ";
					else
						break;
				} while (true);
				response << "];";

			}
		}
		else if ( action == "getversion" ) {
			if ( logged ) {
				ResultSet rs=conn.query("SELECT * FROM page WHERE pathinfo='"+conn.sqlize(pathInfo)+"' AND date='"+request.getParameter("date")+"'");
				rs.next();
				response << rs["body"];
			}
		}
		else if ( action == "external_image_list" ) {
				Logger log("external_image_list");
				log("plop");
			if ( logged ) {
				log("logged");
				response.setContentType("text/javascript");
				struct dirent **entry;
				char *_dir  = strdupa(apacheRequest->filename);
				while ( _dir[strlen(_dir)-1] != '/' && _dir[0] != '\0' ) _dir[strlen(_dir)-1] = '\0';
				String dir=_dir;
				int m = scandir((dir+"/IMG").c_str(),&entry,0,versionsort);
				response.setContentType("text/javascript");
				response << "var tinyMCEImageList = new Array(";
				log(dir+"/IMG");
				log(itostring(m));
				bool first=true;
				for (int n = 0 ; n < m ; ++n ) {
					bool forcedir=false;
					String filename=String("IMG/")+entry[n]->d_name;
					if ( entry[n]->d_type & DT_LNK ) {
						struct stat st;
						if ( stat(String(dir+filename+String("/")).c_str(),&st) == 0  ) {
							if ( S_ISDIR(st.st_mode) )
                                                		forcedir=true;
						}
					}
					if ( entry[n]->d_name[0] == '.' || forcedir || entry[n]->d_type & DT_DIR ) {
						//nop
					}
					else {
						request_rec *rr=ap_sub_req_lookup_file(
						(dir+"IMG/"+String(entry[n]->d_name)).c_str(),
                                                 apacheRequest,
                                                 NULL);
		                                if ( rr ) {
		                                        if (rr->content_type
		                                            && (( rr->content_type[0] == 'i'
		                                            && rr->content_type[1] == 'm'
		                                            && rr->content_type[2] == 'a'
		                                            && rr->content_type[3] == 'g'
		                                            && rr->content_type[4] == 'e' ) ) ) {
								if ( !first ) {
									 response << ",";
								}
								response << "[ \"" << String(entry[n]->d_name) <<  "\",\""<<request.getContextPath()+"/"+filename<<"\"]";
								first = false;

							}
						}

					}
					free(entry[n]);
				}
				if ( m > 0 ) free(entry);
				
				response << ");";

			}
		}
		else if ( action == "media_external_list" ) {
				Logger log("external_image_list");
				log("plop");
			if ( logged ) {
				log("logged");
				response.setContentType("text/javascript");
				struct dirent **entry;
				char *_dir  = strdupa(apacheRequest->filename);
				while ( _dir[strlen(_dir)-1] != '/' && _dir[0] != '\0' ) _dir[strlen(_dir)-1] = '\0';
				String dir=_dir;
				int m = scandir((dir+"/IMG").c_str(),&entry,0,versionsort);
				response.setContentType("text/javascript");
				response << "var tinyMCEMediaList = new Array(";
				log(dir+"/IMG");
				log(itostring(m));
				bool first=true;
				for (int n = 0 ; n < m ; ++n ) {
					bool forcedir=false;
					String filename=String("IMG/")+entry[n]->d_name;
					if ( entry[n]->d_type & DT_LNK ) {
						struct stat st;
						if ( stat(String(dir+filename+String("/")).c_str(),&st) == 0  ) {
							if ( S_ISDIR(st.st_mode) )
                                                		forcedir=true;
						}
					}
					if ( entry[n]->d_name[0] == '.' || forcedir || entry[n]->d_type & DT_DIR ) {
						//nop
					}
					else {
						request_rec *rr=ap_sub_req_lookup_file(
						(dir+"IMG/"+String(entry[n]->d_name)).c_str(),
                                                 apacheRequest,
                                                 NULL);
		                                if ( rr ) {
		                                        if (rr->content_type 
		                                            && !(( rr->content_type[0] == 'i'
		                                            && rr->content_type[1] == 'm'
		                                            && rr->content_type[2] == 'a'
		                                            && rr->content_type[3] == 'g'
		                                            && rr->content_type[4] == 'e'  ) ) ) {
								if ( !first) {
									response << ",";
								}
								response << "[ \"" << String(entry[n]->d_name) <<  "\",\""<<request.getContextPath()+"/"+filename<<"\"]";
								first = false;

							}
						}

					}
					free(entry[n]);
				}
				if ( m > 0 ) free(entry);
				
				response << ");";

			}
		}
		else
			throw ServletException("unknown action");
	}
};

exportServlet(index);
