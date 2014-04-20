/* 
 * Copyright (c) 2006-2011, Guillaume Gimenez <guillaume@blackmilk.fr>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of G.Gimenez nor the names of its contributors may
 *       be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL G.Gimenez SA BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *     * Guillaume Gimenez <guillaume@blackmilk.fr>
 *
 */
#include <raii.H>
#include <json-glib/json-glib.h>

#include "hunspell.hxx"
#include <iconv.h>

using namespace raii;

/*
class Node : public Object {
	typedef enum {	UNKNOWN,
			STRING,
			BOOL,
			DOUBLE,
			INT,
			ARRAY,
			OBJECT } Type
	Type type;
	String name;
	String value_String;
	bool	value_bool;
	double	value_double;
	int	value_int;
	
	
}
*/


String to_utf8(const String& str) {
	iconv_t cd = iconv_open("UTF-8","ISO-8859-1");
	char out[str.size()*2+1];
	char *in=const_cast<char*>(str.c_str());
	char *pi=in,*po=out;
	size_t  si=str.size(),so=str.size()*2;
	iconv(cd,&pi,&si,&po,&so);
	*po='\0';
	iconv_close(cd);
	return out;

}
String from_utf8(const String& str) {
	iconv_t cd = iconv_open("ISO-8859-1","UTF-8");
	char out[str.size()+1];
	char *in=const_cast<char*>(str.c_str());
	char *pi=in,*po=out;
	size_t  si=str.size(),so=str.size();
	iconv(cd,&pi,&si,&po,&so);
	*po='\0';
	iconv_close(cd);
	return out;
}

int htd(int c) {
	if ( c >= '0' && c <= '9' )
		return c-'0';
	if ( c >= 'a' && c <= 'f' )
		return c-'a';
	if ( c >= 'A' && c <= 'F' )
		return c-'A';
	return 0;
}

String eval_string(const String& str) {

	String ret;

	for (unsigned int i = 0 ; i < str.size() ; i++ ) {
		if ( str[i] == '\\' ) {
			int c=0;
			c+=htd(str[++i]);
			c*=16;
			c+=htd(str[++i]);
			ret+=c;
			c=0;
			c+=htd(str[++i]);
			c*=16;
			c+=htd(str[++i]);
			ret+=c;
		}
		else
		ret+=str[i];
	}
	return ret;
}
void eval_node(Map<String,String>& array, const String& varname, JsonNode* node) {

	String type = json_node_type_name(node)?:"(null)";
	static int serial = 0 ;
	String s = ";"+itostring(serial);

	if ( type == "gchararray" ) {
		array[varname+s] = eval_string(json_node_get_string(node)?:"(null)");
	}
	else if ( type == "gboolean" ) {
		array[varname+s] = json_node_get_boolean (node)?"true":"false";
	}
	else if ( type == "gdouble" ) {
		array[varname+s] = ftostring(json_node_get_double(node));
	}
	else if ( type == "gint" ) {
		array[varname+s] = itostring(json_node_get_int(node));
	}
	else 
		array[varname+s] = eval_string(json_node_get_string(node)?:"(null)");

	serial++;

}

void walk_json_object(Map<String,String>& array, const String& varname, JsonObject* object);

void walk_json(Map<String,String>& array, const String& varname, JsonNode* node ) {
	

	if ( node && JSON_NODE_TYPE (node) == JSON_NODE_ARRAY) {

		JsonArray *jarray=json_node_get_array (node);

		GList	*elements=json_array_get_elements (jarray),
			*l;

		for (l = elements; l != NULL; l = l->next) {
			JsonNode *element = (JsonNode*)l->data;
	
			if (JSON_NODE_TYPE (element) == JSON_NODE_ARRAY) {
				walk_json(array,varname+".array",element);
			}
			else if (JSON_NODE_TYPE (element) == JSON_NODE_OBJECT) {
				walk_json_object(array,varname+".object",json_node_get_object(element));
			}
			else
				eval_node(array,varname,element);
	        }
		g_list_free(elements);
	}
	else if ( node && JSON_NODE_TYPE (node ) == JSON_NODE_OBJECT) {
		walk_json_object(array,varname+".object",json_node_get_object(node));
	}
	else
		eval_node(array,varname,node);
}

void walk_json_object(Map<String,String>& array, const String& varname, JsonObject* object) {
	GList	*elements=json_object_get_members (object),
		*l;
	for (l = elements ; l != NULL; l = l->next) {
		const char* member = (const char*)l->data;
		JsonNode *element = json_object_get_member(object,member);
		if ( JSON_NODE_TYPE(element) == JSON_NODE_ARRAY ) {
			walk_json(array,varname+"."+member+".array",element);
		}
		else if ( JSON_NODE_TYPE(element) == JSON_NODE_OBJECT ) {
			walk_json_object(array,varname+"."+member+".object",json_node_get_object(element));
		}
		else
				eval_node(array,varname+"."+member,element);
	}
	g_list_free(elements);
}

String getValue(const Map<String,String>& array, const String& var) {
	for ( Map<String,String>::const_iterator it=array.begin(); it != array.end() ; ++it) {
		String m=String("^")+var+";";
		if ( it->first.matches(m) )  return it->second;
	}
	return "";
	
}
Vector<String> getValues(const Map<String,String>& array, const String& var) {
	Vector<String> vals;
	for (Map<String,String>::const_iterator it=array.begin(); it != array.end() ; ++it) {
		String m=String("^")+var+";";
		if ( it->first.matches(m) )  vals.push_back(it->second);
	}
	return vals;
	
}

class SERVLET(test) : public HttpServlet {

	void service(HttpServletRequest& request, HttpServletResponse& response) {

		if ( request.getSession()->getUser().empty() )
			throw ServletException("vous devez être authentifié!");

		String body = request.getBody();

		static bool firstrun=true;

		if ( firstrun ) { g_type_init (); firstrun=false; }
		GError *error=NULL;
/*
			body="{\"plop2\":1,\"plop3\":3.14,\"plop\":true,\"id\":\"c0\",\"method\":\"checkWords\",\"params\":[\"fr\","
				"[\"blackmilk\",\"La\",\"boisson\",\"au\",\"bon\",\"go\\\\u00fbt\","
				"\"de\",\"logiciels\",\"libres\"]]}";
*/
//		body="{\"id\":\"c0\",\"method\":\"getSuggestions\",\"params\":[\"fr\",\"esp\u00e9riez\"]}";
		JsonParser* parser = json_parser_new();
		if ( ! parser ) throw ServletException("parser is null");

			json_parser_load_from_data(parser,body.c_str(),-1,&error);

				if ( error ) {
					g_object_unref (parser);
					throw ServletException(error->message);
				}


				JsonNode * node = json_parser_get_root(parser);
				if ( !node ) {
					g_object_unref (parser);
					throw ServletException("root node is null");
				}


				Map<String,String> array;
				walk_json(array,"root",node);


		//	json_node_free (node);

		g_object_unref (parser);



		String method = getValue(array,"root.object.method");
		if ( method == "checkWords" ) {

			Hunspell speller("/usr/share/myspell/dicts/fr_FR.aff",
					 "/usr/share/myspell/dicts/fr_FR.dic");

			Vector<String> strings = getValues(array,"root.object.params.array.array");
			for ( Vector<String>::iterator it = strings.begin() ; it != strings.end() ;) {
				int dp = speller.spell(from_utf8(*it).c_str());
				if ( dp ) it = strings.erase(it);
				else ++it;
			}

			String resp="{\"id\":null,\"result\":[";
			for ( Vector<String>::iterator it = strings.begin() ; it != strings.end() ;) {
				resp=resp+"\""+to_utf8(*it)+"\"";
				if ( ++it != strings.end() ) resp += ",";
			}
			resp+="],\"error\":null}";
			response << resp;
			
		}
		else if ( method == "getSuggestions" ) {
			Hunspell speller("/usr/share/myspell/dicts/fr_FR.aff",
					 "/usr/share/myspell/dicts/fr_FR.dic");

			String resp="{\"id\":null,\"result\":[";
			Vector<String> strings = getValues(array,"root.object.params.array");
			for ( Vector<String>::iterator it = strings.begin() ; it != strings.end() ;++it) {

				if ( *it == "fr" ) continue;
				char **wlst;
				int ns = speller.suggest(&wlst,from_utf8(*it).c_str());
				for (int i=0; i<ns;) {
					resp=resp+"\""+to_utf8(wlst[i])+"\"";
					if ( ++i < ns ) resp += ",";
				}
				resp+="],\"error\":null}";
				speller.free_list(&wlst,ns);
			}
			response << resp;


		}



	}
};

exportServlet(test);
