#include "interface/StandardIncludes.hpp"


using namespace std;


#ifndef EVTBLD_H
#define EVTBLD_H
#include "interface/Logger.hpp"

//typedef unsigned int WORD;
typedef uint32_t WORD;

class EventParser{
public:
	/* this class converts the streams events in ROOT TTree files
	 */
	// --- Constructor
	EventParser();
	// --- Destructor
	~EventParser();

};

class dataType{
	//this is a dynamic array of char*. 
	//string is null terminated = bad!!! 
	//the syntax is the same of string
private:
	void *dataStream_;
	unsigned int size_; // store the used size of dataStream
	unsigned int reserved_; //store the actual dimension in memory of dataStream
public:
	// --- Constructor
	dataType();
	//dataType(const char*);//init with a null terminated string
	dataType(int N, void* );//init with a dataType
	// --- Destructor
	~dataType();
	// --- Get size
	unsigned int size(){return size_;}
	// --- reserve N bytes in memory
	void reserve(int N);
	// --- shrink N bytes in memory
	void shrink(int N);
	// --- return stream pointer
	const void* c_str(){return (const void*)dataStream_;}
	void* data(){
		return ( void*)dataStream_;
		}
	// --- append
	void append(void* data, int N);
	// --- append using dataType
	void append(dataType x){return append( x.data(), int(x.size()) ) ;};
	// --- remove [A,B)
	void erase(int A,int B);
	// --- clear
	void clear();
	// --- copy
	void copy(void * data, int N);
	// --- release ownership of data. This will lost control of data and location
	void release();
	//--- Operators
	//dataType& operator+=(dataType &x) { this->append(x); return this;}

};
// --- bool operator -- mv inside the class
const bool operator==(dataType &x, dataType &y);

class EventBuilder : public LogUtility, public Configurable{
// ---binary stream of the event -- 1char = 1byte
dataType myRun_; // dynamic array of char*

bool dumpEvent_; // default true
bool sendEvent_; // default false
int recvEvent_; // if set to true will set also dumpEvent
Logger *dump_; // this is not the Logger. This will be used to dump the event, and will be set in binary mode.
bool isRunOpen_;
WORD lastRun_;

map<WORD,pair<int,dataType> > runs_; //store incomplete runs if in recv mode. RUNNUM -> NMerged, RunStream

	void MergeRuns(dataType &run1,dataType &run2 ); //TODO
	
public:
	/* this class contains the raw event.
	 * It performs simple operations, like merging all the event informations, with base checks on the counters
	 * In case of errors returns an exception that will trigger a reset of the counters
	 */
	// --- Constructor
	EventBuilder();
	// --- Destructor
	~EventBuilder();
	// 
	void AppendToStream();
	const void* GetStream(){ return myRun_.c_str();}
	int  GetSize(){return myRun_.size();}
	void Config(Configurator&); // TODO --check that all is complete
	void Init();//TODO -- check
	void Clear(); //TODO -- check
	inline void Dump(dataType&run) {dump_->Dump(run);};
	void OpenRun(WORD runNum);
	inline void OpenRun() { OpenRun( lastRun_ + 1 ) ; }; 
	void CloseRun( );
	void AddEventToRun(dataType &event ); 
	// merge in Run1, Run2. check i runNum is to be dumped
	void MergeRuns(dataType &run2 ) ;//{WORD runNum=ReadRunNum(run2); MergeRuns(runs_[runNum].second,run2); runs_[runNum].first++; return;} ;

	// ---  this will be used by hwmanager to convert the output of a board in a stream
	// ---  appends a header and trailer
	static dataType WordToStream(WORD x);
	static vector<WORD> StreamToWord(dataType &x);
	static vector<WORD> StreamToWord(void*v,int N);
	static WORD 	ReadRunNum(dataType &x);
	static WORD 	ReadRunNevents(dataType &x);
	static WORD 	ReadEventNboards(dataType &x);
	static dataType BoardHeader(WORD boardId);
	static dataType BoardTrailer(WORD boardId);
	static dataType EventHeader();
	static dataType EventTrailer();
	static dataType RunHeader();
	static dataType RunTrailer();
	static dataType BoardToStream(WORD boardId,vector<WORD> &v);
	static dataType MergeEventStream(dataType &x,dataType &y);
	static long long IsBoardOk(dataType &x,WORD boardId); // return 0 if NOT, otherwise the NBytes of the TOTAL BOARD STREAM 
	static long long IsBoardOk(void *v,int MaxN,WORD boardId); // if id==NULL don't check consistency for that id; MaxN is the space where it is safe to look into (allocated)
	static long long IsEventOk(dataType &x); // return 0 if NOT, otherwise the NBytes of the TOTAL Event STREAM
	static inline long long IsEventOk(void *v, int N){ dataType tmp(N,v);long long R= IsEventOk(tmp); tmp.release(); return R;};
};

#endif
