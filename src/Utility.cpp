#include "interface/Utility.hpp"
#include <sstream>

string Utility::AsciiData(void *data,int N,int M)
{
string mex="";
char buf[4];
for(int i=0;i<N ;i++)
	{
	sprintf(buf,"%02X",((unsigned char*)data)[i] );
	mex += buf;
	if ((i%M)==M-1) mex += "\n";
	else mex += " ";
	}
if( (N-1)%M != M-1 ) mex += "\n";
return mex;
}

string Utility::AsciiDataReadable(void *data,int N)
{
string mex="";
char buf[4];
for(int i=0;i<N ;i++)
	{
	char c=((unsigned char*)data)[i];
	if (  ('a' <= c && c<='z' ) ||
	      ('A' <= c && c<='Z' ) ||
	      ('0' <= c && c<='9' ) )
		{ mex += c; continue; }
	sprintf(buf,"%02X",((unsigned char*)data)[i] );
	mex += " '" ;
	mex += buf;
	mex += "' ";
	}
return mex;
}

string Utility::AsciiData(void *data,void *data2,int N,int M)
{
string mex="";
string mex1=AsciiData(data,N,M);
string mex2=AsciiData(data2,N,M);

// put together lines
while( !mex1.empty() && !mex2.empty() )
	{
	size_t pos1=mex1.find("\n");
	size_t pos2=mex2.find("\n");
	mex += mex1.substr(0,pos1) + "     " + mex2.substr(0,pos2) + "\n";
	mex1.erase(0,pos1+1);
	mex2.erase(0,pos2+1);
	}
return mex;
}

string Utility::ConvertToString(int N){
	std::ostringstream ostr; //output string stream
	ostr << N; //use the string stream just like cout,
	std::string theNumberString = ostr.str(); //the str() function of the stream 
	return theNumberString;
}

long Utility::timevaldiff(struct timeval *starttime, struct timeval *finishtime)
{
  long usec;
  usec=(finishtime->tv_sec-starttime->tv_sec)*1000000;
  usec+=(finishtime->tv_usec-starttime->tv_usec);
  return usec;
}

long Utility::timestamp(struct timeval *time, time_t *ref){
		
  long time_msec;
  time_msec=(time->tv_sec-*ref)*1000;
  time_msec+=time->tv_usec/1000;
  return time_msec;
}

int Utility::hibit(WORD n) {
  return (n & 0x80000000) ? 31 : hibit((n << 1) | 1) - 1;
}

void Utility::SpaceToNull(int N,void*data,bool first)
{
	for(int i=0;i<N ;i++) 
		if( ((char*)data)[i] ==' ') 
		{
			((char*)data)[i] =='\0';
			if (first) break;
		}
	return ;
}
int Utility::FindNull(int N,void*data,int iPos)
{
	int counter=0;
	for(int i=0;i<N ;i++) 
		if( ((char*)data)[i] =='\0') 
		{
			counter++;
			if(counter == iPos && i+1<N) return i+1;
		}
	return -1;
}
