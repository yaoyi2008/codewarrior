#include "file_ops.h"
#include "mem_ops.h"
#include "string_ops.h"
#include "../lib/libmongoose/mongoose.h"
#include "html_entities.h"
#include "../lib/BSD/strsec.h"
#include "validate.h"
#define MAX_file_len 1000000

char *file_content(const char * filename) 
{
    FILE *fp = fopen(filename, "r");
    size_t file_size;
    long pos;
    char *file_contents;

    	if (!fp)
        	return NULL;
    	fseek(fp, 0L, SEEK_END);
    	pos = ftell(fp);

    	if (pos < 0) 
	{
        	fclose(fp);
        	return NULL;
    	}

    	file_size = pos;
    	rewind(fp);
    	file_contents = xmalloc(sizeof(char) * (file_size + 1));

    	if (!file_contents) 
	{
        	fclose(fp);
        	return NULL;
    	}

    	if (fread(file_contents, file_size, 1, fp) < 1) 
	{
        	if (ferror(fp)) 
		{
            		fclose(fp);
            		XFREE(file_contents);
            		return NULL;
        	}
    	}

   	fclose(fp);
    	file_contents[file_size] = '\0';

    	return file_contents;
}

//read lines of file
char *ReadLines(char * NameFile)
{
	FILE * fh;
	static char buffer[MAX_file_len];

	memset(buffer,0,MAX_file_len-1);

	fh = fopen(NameFile, "rb");

	if( fh == NULL )
	{

		DEBUG("error in to open() file");
		perror("Error ");
		exit(-1); 	 
		
	}

	if(fseek(fh, 0L, SEEK_END)==0)
	{
    		size_t s = ftell(fh);
    		rewind(fh);

    		if ( buffer != NULL && s < MAX_file_len )
    		{
      			if(!fread(buffer, s, 1, fh))
				DEBUG("error \n");
    		}
	}

 
	if( fclose(fh) == EOF )
	{
		DEBUG("Error in close() file %s",NameFile);
		exit(1);
	}

	fh=NULL;

	char *tmp=buffer;
	
	return tmp;
}




// search matchs in file
void *Search_for(char * NameFile,char *regex)
{
        long int count=1;
	size_t LineSize=0,CounterSize;
	char *lineBuffer=xcalloc(1,1);
 	char *linescount=xcalloc(1,1);
	char tmpline[3128],counter[8],line[3127];
	FILE * arq;

	memset(tmpline,0,3127);

	arq = fopen(NameFile, "r");

// todo think implement fcntl() ,toctou mitigation...
	if( arq == NULL )
	{
		DEBUG("error in to open() file"); 	 
		exit(1);
	}

	memset(line,0,3126);

	while( fgets(line,sizeof line,arq) )  
	{
		if(match_test(line,regex)==true)
		{
			
		 	LineSize+=3128+512;
			lineBuffer=xrealloc(lineBuffer,LineSize);
			snprintf(tmpline,3128,"Line: %ld -  %s",count,line);
			strlcat(lineBuffer,tmpline,LineSize);
			CounterSize+=strlen(linescount)+9;
			linescount=xrealloc(linescount,CounterSize);
			snprintf(counter,8,"%ld,",count); // add number of lines in URL by param example: 1,21,11,56..
			strlcat(linescount,counter,CounterSize);
			memset(counter,0,8);
		}
		LineSize=0;
		CounterSize=0;	
		count++;
	}

 
	if( fclose(arq) == EOF )
	{
		DEBUG("Error in close() file %s",NameFile);
		exit(1);
	}

	arq=NULL;

// anti XSS
	char *sanitize=html_entities(lineBuffer);
	char *pack[2];

	if(strlen(linescount)>=2)
	{
		pack[0]=strdup(sanitize);
		pack[1]=strdup(linescount);
	} else {
		pack[0]=NULL;
		pack[1]=NULL;
	}

	void *back=xmalloc(2*sizeof(char *));

	memcpy(back, &pack, sizeof(pack));



	return back;
}

/*
 * Open Module egg, and search by matchs and construct the report
 *
 * */
bool fly_to_analyse(char *path, char *config, char * extension, struct mg_connection *c)
{
	bool report_status=false;
	int result=0,sz=0;
	char *language=get_extension(extension);
	char *p=ReadLines(config);
	char *last=p,*report=NULL;
	char title[128],description[512],reference[512],match[1024],relevance[512];	
		
	while(!result && strlen(last)>16)
	{
		switch (parse_eggs(&p, &last)) 
		{
			case TITLE:
					sz = p - last;
					memset(title,0,127);
					snprintf(title,128,"%.*s", sz, last);
					strlcpy(title,ClearStr(title,10),sizeof(title));
				break;

			case DESCRIPTION:
				
 					sz = p - last;
					memset(description,0,511);
					snprintf(description,512,"%.*s", sz, last);
					strlcpy(description,ClearStr(description,16),sizeof(description));
				break;

			case REFERENCE:

					sz = p - last - 1;
					memset(reference,0,511);
					snprintf(reference,512,"%.*s", sz, last);
					strlcpy(reference,ClearStr(reference,14),sizeof(reference));
				break;


			case RELEVANCE:

					sz = p - last;
					memset(relevance,0,511);
					snprintf(relevance,512,"%.*s", sz, last);
					strlcpy(relevance,ClearStr(relevance,14),sizeof(relevance));
				break;

			case MATCH:
					sz = p - last;
					memset(match,0,1023);
					snprintf(match,1024,"%.*s", sz, last);
					strlcpy(match,ClearStr(match,10),sizeof(match));

					char **arg=(char **)Search_for(path,match);
					char **result2=arg;

// TODO* need validate before print out
					if(result2[0]!=NULL)
					{
//get relevance icon
						char *icon_alert=get_relevance_icon(relevance);
// XSS mitigation
						char *title_clean=html_entities(title),*description_clean=html_entities(description),*relevance_clean=html_entities(relevance);
						char *reference_clean=html_entities(reference),*match_clean=html_entities(match),*path_clean=html_entities(path);

// construct the report 
						int sizereport=strlen(title_clean)+strlen(result2[1])+strlen(description_clean)+strlen(relevance_clean);
						sizereport+=(strlen(reference_clean)*2)+strlen(match_clean)+strlen(result2[0])+(strlen(path_clean)*2)+strlen(config)+(strlen(language)*2)+487;
						sizereport+=strlen(icon_alert);
						report=xmalloc(sizereport*sizeof(char));
						memset(report,0,sizereport-1);
						snprintf(report,sizereport,"<img src=\"img/kunai.png\" width=\"80\" height=\"60\" align=\"center\" ><div class=\"path well\"><b>Title:</b> %s<br> <b>Description:</b> %s<br> <b>Relevance:</b> %s<br> <b>Reference:</b> <a class=\"fancybox fancybox.iframe\" href=\"%s\">%s</a><br><b>Match:</b> %s <br><b>Path:</b> <a class=\"fancybox fancybox.iframe\" href=\"viewcode.html?path=%s&lang=%s&lines=%s\">%s</a><br><b>Module:</b> %s <img src=\"img/%s\" width=\"80\" height=\"60\" align=\"right\" ></div><pre type=\"syntaxhighlighter\" class=\"brush: %s;\" >%s</pre><br>",title_clean,description_clean,relevance_clean,reference_clean,reference_clean,match_clean,path_clean,language,result2[1],path_clean,config,icon_alert,language,result2[0]);


	
// send result to web socket
						mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, report, sizereport);
						
						XFREE( result2[0]);
						XFREE( result2[1]);
						XFREE( icon_alert);
						XFREE( title_clean);
						XFREE( description_clean);
						XFREE( relevance_clean);
						XFREE( reference_clean);
						XFREE( match_clean);
						XFREE( path_clean);
						XFREE(report);

						report_status=true;
					}
				break;


			case END:
				result=1;	
				break;
    		}
	
	
	}
	
	if(strlen(last)>16)
		XFREE( last);

	XFREE( language);

	return report_status;
}


/*
 * Open dir on recursive mode, get file by extension...
 *
 * */
void warrior_start (const char * dir_name, char * extension, char * config,  struct mg_connection *c)
{
	DIR * d;
	char tmp_path[512];	
	short counter=0;

 	d = opendir (dir_name);
 
	if ( d == NULL) 
	{
		DEBUG ("Cannot open directory '%s': %s\n", dir_name, strerror (errno));
 		exit (EXIT_FAILURE);
	}

	while (1) 
	{
		struct dirent * entry;
 		const char * d_name;

		entry = readdir (d);

		if (! entry) 
		{
			break;
		}

		d_name = entry->d_name;

	
// TODO* i need improve that extension check
		if(strcmp(d_name,".") && strcmp(d_name,"..") && match_test(d_name,extension)==true)
		{
			snprintf(tmp_path,512,"%s/%s",dir_name,d_name);
			bool result=fly_to_analyse(tmp_path, config, extension, c);

			if(result!=false )
			{
// TODO: limit this
				counter++;
				memset(tmp_path,0,511);
				if(counter==20)
					sleep(15000),counter=0;
			}

			

		}


	        if (entry->d_type & DT_DIR) 
		{

            
	            if (strcmp (d_name, "..") != 0 && strcmp (d_name, ".") != 0) 
		    {
	  		int path_length;
			char path[1024];
 
			path_length = snprintf (path, 1024, "%s/%s", dir_name, d_name);


     	                if (path_length >= 1023) 
			{
               		     DEBUG("Path length has got too long.\n");
               		     exit(0);
               		}

                	warrior_start (path,extension,config,c);
	         }
	}
    }

    if(closedir(d)) 
    {
       	perror("Could not close\n");
       	exit(0);
    }

    
}

/*
 * Open and Show the source code with highlights
 *
 * */
void view_source(struct mg_connection *c, char *pathdirt)
{
	char pathdirt2[4094];
	memset(pathdirt2,0,4093);
	strlcat(pathdirt2,pathdirt,4094);
	
	if(pathdirt!=NULL && url_viewcode_valid(pathdirt2)==false)
	{
		DEBUG("error %s\n",pathdirt2);
		mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, "invalid path", 12);
		return;
			
	}

    	int sz;
    	int result=0;
	char *p = pathdirt;
    	char *last = p;
    	char path[3048],lang[32],lines[256];

    	memset(path,0,3047);
    	memset(lang,0,31);
    	memset(lines,0,255);

// parse the URL and get inputs
    	while(!result )
   	 switch (parse_viewcode(&p, &last)) 
	 {
       	  case PATH:
         	sz = p - last - 1;
           	snprintf(path,3048,"%.*s", sz, last);
	    	memmove(path, path+5, strlen(path));
            	break;

          case LANG:
          	sz = p - last - 1;
            	snprintf(lang,32,"%.*s", sz, last);
	    	memmove(lang, lang+5, strlen(lang));
            	break;

          case LINES: 
            	sz = p - last - 1;
            	snprintf(lines,256,"%.*s", sz, last);
	    	short lenstr=strlen(lines);
	    	lines[lenstr-1]='\0';	    
	    	memmove(lines, lines+6, lenstr);    	
            	break;

          case END:
	    	result=1;	
            	break;
    	 }

	FILE * fh;
	static char buffer[MAX_file_len];

	memset(buffer,0,MAX_file_len-1);

// DEBUG("%s %s %s\n",path,lang,lines);

	char decode_path[3048];
	memset(decode_path,0,3047);
	URLdecode(path,decode_path);

	fh = fopen(decode_path, "rb");

	if( fh == NULL )
	{
		DEBUG("%s\n",path);

		perror("Error ");
		exit(-1); 	 
		
	}

	if(fseek(fh, 0L, SEEK_END)==0)
	{
    		long s = ftell(fh);
    		rewind(fh);

    		if ( buffer != NULL && s < MAX_file_len )
    		{
      			if(!fread(buffer, s, 1, fh))
				perror("error \n");
    		}
	}

 
	if( fclose(fh) == EOF )
	{
		printf("Error in close() file %s",path);
		exit(1);
	}

	fh=NULL;

	int len_output=MAX_file_len+731+256;
	char *output=xmalloc(len_output);
	char *code=html_entities(buffer);
	char *lines_clean=html_entities(lines);
	char *lang_clean=html_entities(lang);
	char *path_clean=html_entities(path);
	memset(output,0,len_output-1);

	snprintf(output,len_output,"<b>Path:</b> %s<br><pre type=\"syntaxhighlighter\" class=\"brush: %s; highlight: [%s];\" >%s</pre>",path_clean,lang_clean,lines_clean,code);

// send source code to viewcode.html
	mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, output, len_output);

	XFREE(code);
	XFREE(lines_clean);
	XFREE(path_clean);
	XFREE(lang_clean);
	XFREE(output);
//	XFREE(pathdirt);
}


/*
 *  This function search in recursive per extension, and open each file and find one match
 *
 * */
void warrior_sink (const char * dir_name, char * extension, char *sink,  struct mg_connection *c)
{
	DIR * d;
	char tmp_path[2048];	
	char *language=get_extension(extension);
	
 	d = opendir (dir_name);
 
	if ( d == NULL) 
	{
//		closedir(d);
		DEBUG ("Cannot open directory '%s': %s\n", dir_name, strerror (errno));
 		exit (EXIT_FAILURE);
	}

	while (1) 
	{
		struct dirent * entry;
 		const char * d_name;

		entry = readdir (d);

		if (! entry) 
		{
			break;
		}

		d_name = entry->d_name;

	
// TODO* i need improve that extension check
		if(strcmp(d_name,".") && strcmp(d_name,"..") && match_test(d_name,extension)==true)
		{
			snprintf(tmp_path,2048,"%s/%s",dir_name,d_name);
			char **argv=(char **)Search_for(tmp_path,sink);
			char **result=(char **)argv;

			if(result[0]!=NULL)
			{
				int sizereport=strlen(result[0])+strlen(result[1])+2048+strlen(sink)+strlen(tmp_path);
				char *report=xmalloc(sizereport);
				memset(report,0,sizereport-1);		
				char *path_clean=html_entities(tmp_path);		
				char *sink_clean=html_entities(sink);

				snprintf(report,sizereport,"<img src=\"img/kunai.png\" width=\"80\" height=\"60\" align=\"center\" ><div class=\"path well\"><b>Sink:</b> %s<br> <b>Lines:</b> %s<br><b>Path:</b> <a class=\"fancybox fancybox.iframe\" href=\"viewcode.html?path=%s&lang=%s&lines=%s\">%s</a><br></div><pre type=\"syntaxhighlighter\" class=\"brush: %s;\" >%s</pre><br>",sink_clean,result[1],path_clean,language,result[1],path_clean,language,result[0]);
// send result to web socket
				mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, report, strlen(report));
				memset(tmp_path,0,2047);

				XFREE(report);
				XFREE(result[0]);
				XFREE(result[1]);
				XFREE(path_clean);
				XFREE(sink_clean);
				XFREE(result);
			}

			
		}


	        if (entry->d_type & DT_DIR) 
		{

            
	            if (strcmp (d_name, "..") != 0 && strcmp (d_name, ".") != 0) 
		    {
	  		int path_length;
			char path[2048];
 
			path_length = snprintf (path, 2048, "%s/%s", dir_name, d_name);


     	                if (path_length >= 2047) 
			{
               		     DEBUG("Path length has got too long.\n");
               		     exit(0);
               		}

                	warrior_sink (path,extension,sink,c);
	         }

	         
	}
    }

    if(closedir(d)) 
    {
        perror("Could not close\n");
        exit(0);
    }


    XFREE( language);

}






/**
 *
 *  This function list tree of files...
 *
 */
void warrior_tree (const char * dir_name, char * extension,  struct mg_connection *c)
{
	DIR * d;
	char tmp_path[1024];	
	char *language=get_extension(extension);

//debug printf("%s\n",dir_name);
 	d = opendir (dir_name);
 
	if ( d == NULL) 
	{
//		closedir(d);
		DEBUG ("Cannot open directory '%s': %s\n", dir_name, strerror (errno));
 		exit (EXIT_FAILURE);
	}

	while (1) 
	{
		struct dirent * entry;
 		const char * d_name;

		entry = readdir (d);

		if (! entry) 
		{
			break;
		}

		d_name = entry->d_name;

	
// TODO* i need improve that extension check
		if(strcmp(d_name,".") && strcmp(d_name,"..") && match_test(d_name,extension)==true)
		{
			snprintf(tmp_path,1024,"%s/%s",dir_name,d_name);
			char tree_element[2048];			
			memset(tree_element,0,2047);
			char *path_clean=html_entities(tmp_path);


			snprintf(tree_element,2048,"<div class=\"path well\"><b>Path:</b> <a class=\"fancybox fancybox.iframe\"  href=\"viewcode.html?path=%s&lang=%s&lines=1\">%s</a><br></div>",path_clean,language,path_clean);
			mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, tree_element, 2048);
			XFREE( path_clean);
		}


	        if (entry->d_type & DT_DIR) 
		{

            
	            if (strcmp (d_name, "..") != 0 && strcmp (d_name, ".") != 0) 
		    {
	  		int path_length;
			char path[1024];
 
			path_length = snprintf (path, 1024, "%s/%s", dir_name, d_name);


     	                if (path_length >= 1023) 
			{
               		     DEBUG("Path length has got too long.\n");
               		     exit(0);
               		}

                	warrior_tree (path,extension,c);
	         }
	}
    }

    if(closedir(d)) 
    {
        perror("Could not close\n");
        exit(0);
    }


    XFREE( language);
}

// write string in file
void Write_File(char *file,char *str)
{
	FILE *arq;
 
	arq=fopen(file,"w"); 

	if ( arq == NULL ) 
	{
//		fclose(arq);
		DEBUG("error in WriteFile() %s",file); 
		exit(1);
	}

	fprintf(arq,"%s",str); 

	if( fclose(arq) == EOF )
	{
		DEBUG("error in Write() file %s",file);
		exit(1);
	}
	arq=NULL;
}