#include "interface/EventBuilder.hpp"
#include "interface/DataType.hpp"
#include "interface/Utility.hpp"
#include "interface/Board.hpp"
#include <sstream>

#define EB_DEBUG
//#define TIME_DEBUG



// ---------- Event Builder
EventBuilder::EventBuilder()
{
	//mySpill_.reserve(2048); //reserve 1K for each stream
	dumpEvent_=true;
	sendEvent_=false;
	recvEvent_=0;
	isSpillOpen_=false;
	//lastSpill_=100;
	ResetSpillNumber();
	// Init dumper
	dump_=new Logger();
	// dump_->SetFileName("/tmp/dump.gz"); // rewritten by config
	dump_->SetCompress();
	dump_->SetBinary();
	dump_->SetAsync(); // asyncronous dumping
	merged_=0;
	lastBadSpill_=0;  // spill n. starts from 1
	goodEventsInThisRun_=0;
	badSpillsInThisRun_=0;
	async_=true; // async utilts, for dqm
}
EventBuilder::~EventBuilder()
{
	Clear();
	delete dump_;
}


// ---- STATIC EVENT BUILDER
//


void EventBuilder::WordToStream(dataType&R, WORD x)
{
	R.append( (void*)&x,WORDSIZE);
	return ;
}

void EventBuilder::BoardHeader(dataType &R, BoardId id)
{
	//R.reserve(2*WORDSIZE);
	R.append((void*)"BRDH\0\0\0\0\0\0\0\0",WORDSIZE); //WORDSIZE<12
	//dataType 
	//
	//compute shifts for BitMasks: 
	//crate id starts from 1->n independently from the bit mask
	int myCrateIdShift=GetCrateIdShift();
	int myBoardIdShift=GetBoardIdShift();
	int myBoardTypeShift=GetBoardTypeShift();

	WORD myId= 	( (id.crateId_<<myCrateIdShift)       & GetCrateIdBitMask()     ) | 
			( (id.boardId_<<myBoardIdShift)     & GetBoardIdBitMask()     ) |  
			( (id.boardType_<<myBoardTypeShift) & GetBoardTypeIdBitMask() )  ;
	WordToStream(R,myId);
#ifdef EB_DEBUG
	ostringstream s;
	s<<"[EventBuilder]::[BoardId]::[DEBUG] CrateId="<< id.crateId_
		<<" BID="<<id.boardId_
		<<" TYP="<<id.boardType_
		<<" WORD="<<std::hex<< myId<<std::dec;
	printf("%s\n",s.str().c_str());
#endif

	return ;
}

void EventBuilder::BoardTrailer(dataType&R)
{
	//R.reserve(WORDSIZE);
	R.append((void*)"BRDT\0\0\0\0\0\0\0\0",WORDSIZE); // WORDSIZE<12
	return ;
}


// [HEAD][Nbytes][ ----- ][TRAILER]
// [HEAD]="BRDH"+"BRDID"+[BOADSIZE] - WORD-WORD
void EventBuilder::BoardToStream(dataType &R ,BoardId id,vector<WORD> &v)
{
	//R.reserve(v.size()*4+12);// not important the reserve, just to avoid N malloc operations
	dataTypeSize_t oldSize=R.size();
	BoardHeader(R, id );
	WORD N= (v.size() + BoardHeaderWords() + BoardTrailerWords() )*WORDSIZE  ;
	WordToStream(R,N)  ; 
	for(unsigned long long i=0;i<v.size();i++) WordToStream(R,v[i])  ;
	BoardTrailer(R) ;
	dataTypeSize_t newSize=R.size();
	if(newSize-oldSize != (dataTypeSize_t)N ) 
		printf("[EventBuilder]::[BoardToStream] Size is inconsistent\n");

	return ;
}

void EventBuilder::EventHeader( dataType &R)
{
	//R.reserve(WORDSIZE);
	R.append((void*)"EVTH\0\0\0\0\0\0\0\0",WORDSIZE);
	return ;
}

void EventBuilder::EventTrailer(dataType &R)
{
	//R.reserve(WORDSIZE);
	R.append((void*)"EVNT\0\0\0\0\0\0\0\0",WORDSIZE);
	return ;
}

void EventBuilder::SpillHeader(dataType &R)
{
	//R.reserve(WORDSIZE);
	R.append((void*)"SPLH\0\0\0\0\0\0\0\0",WORDSIZE);
	return ;
}

void EventBuilder::SpillTrailer(dataType &R)
{
	//R.reserve(WORDSIZE);
	R.append((void*)"SPLT\0\0\0\0\0\0\0\0",WORDSIZE);
	return ;
}

void EventBuilder::MergeEventStream(dataType &R,dataType &x,dataType &y){ 
#ifdef EB_DEBUG
	printf("[EventBuilder]::[MergeEvents]::[DEBUG] MergeEventStream\n");
#endif
	// takes two streams and merge them independently from the content
	// R should be empty
	if (R.size() >0 ) {
			//Log("[EventBuilder]::[MergeEventStream] Return Size is more than 0", 1); // cannot Log, private
			printf("[EventBuilder]::[MergeEventStream] Return Size is more than 0\n");
			throw logic_exception();
			}
	//dataType R;
	R.append(x);

	WORD nboards1=ReadEventNboards(x);
	WORD nboards2=ReadEventNboards(y);
		
	WORD eventNum1=ReadEventNumber(x);
	WORD eventNum2=ReadEventNumber(y);

#ifdef EB_DEBUG
	printf("[EventBuilder]::[MergeEvents]::[DEBUG] nBoard: %u;%u evenNum: %u==%u\n",nboards1,nboards2,eventNum1,eventNum2);
#endif
		
	if(eventNum1 != eventNum2) {
		R.clear();
		return;
		}

	long size1=IsEventOk(x);
	long size2=IsEventOk(y);

	WORD *ptrNboards=(WORD*)R.data() + EventNboardsPos();
	*ptrNboards=nboards1+nboards2;
	R.reserve(size1+size2); // minus hedrs but it is just a reserve
#ifdef EB_DEBUG
	printf("[EventBuilder]::[MergeEvents]::[DEBUG] Merge Events erase");
#endif
	R.erase(size1-WORDSIZE*EventTrailerWords(), size1); // delete trailer
#ifdef EB_DEBUG
	printf("[EventBuilder]::[MergeEvents]::[DEBUG] Merge Events erase Done");
#endif

	WORD*ptr2= (WORD*)y.data() + EventHeaderWords() ; // content

	R.append( (void*)ptr2, size2 - WORDSIZE*EventHeaderWords() ) ; // remove H, copy T
	// update size
	WORD *sizepos=(WORD*)R.data() + EventSizePos();
	*sizepos= (WORD)R.size();
#ifdef EB_DEBUG
	printf("[EventBuilder]::[MergeEvents]::[DEBUG] MergeEventStream. Done\n");
#endif
	return ;
}


vector<WORD>	EventBuilder::StreamToWord(void*v,int N){
	dataType myStream(N,v);
	vector<WORD> R = StreamToWord( myStream);
	myStream.release();
	return R;
}
vector<WORD>	EventBuilder::StreamToWord(dataType &x){
	vector<WORD> R;
	if( x.size() % WORDSIZE  != 0 ) 
		{
		printf("[EventBuilder]::[StreamToWord] Error in rounding last byte\n",2);
		}
	dataTypeSize_t nWord=x.size() /WORDSIZE ;
	for(unsigned long long int n=0; n<nWord ; n++)
		{
		R.push_back( *( (WORD*)x.data() + n) );
		}
	// check rounding
	return R;
}

dataTypeSize_t EventBuilder::IsBoardOk(dataType &x){

#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsBoardOk] START\n");
#endif
	BoardId empty;
	dataType H;BoardHeader(H,empty);
	dataType T;BoardTrailer(T);
	
	// Look in the Header for the 
	vector<WORD> Header  = StreamToWord( H );
	vector<WORD> Trailer = StreamToWord( T );
	if(x.size() < BoardHeaderWords()*WORDSIZE ){
		printf("[EventBuilder]::[IsBoardOk] Header size is wrong: x.size()=%u >= BHW *WS %u\n",x.size(),BoardHeaderWords()*WORDSIZE);
		 return 0;
		}
	vector<WORD> myHead  = StreamToWord( x.data(), BoardHeaderWords()*WORDSIZE ); // read the first three
	
	if (myHead[0]  != Header[0] ) {
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsBoardOk] Header is wrong\n");
	printf("[EventBuilder]::[IsBoardOk] Header = %u==%u\n",myHead[0],Header[0]);
	printf("[EventBuilder]::[IsBoardOk] Header = %c%c%c%c==%c%c%c%c\n",((char*) &myHead[0])[0],((char*) &myHead[0])[1],((char*) &myHead[0])[2],((char*) &myHead[0])[3],
		((char*)&Header[0])[0],((char*)&Header[0])[1],((char*)&Header[0])[2],((char*)&Header[0])[3]
		);
#endif
		return 0;
		}


	// the the N of bytes of the stream
	if (myHead.size() < BoardHeaderWords()) 
			{
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsBoardOk] Header is size wrong,second check\n");
#endif
			return 0;
			}
	WORD NBytes = myHead[BoardSizePos()]; // Start Counting from 0
	WORD NWords  = NBytes / WORDSIZE;

	if ( x.size() < NBytes) {
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsBoardOk] Size is inconsistent\n");
#endif
			return 0;
			}
	vector<WORD> myWords = StreamToWord( x.data(), NBytes  ); //
	//check trailer
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsBoardOk] STREAM TO WORD TAKE: %u bites");
	printf("[EventBuilder]::[IsBoardOk] MyWords=%u NWords=%u\n",myWords.size(),NWords);
#endif
	if (myWords[NWords-1] != Trailer[0] ) 
		{
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsBoardOk] Trailer is inconsistent: %u == %u\n",myWords[NWords-1] ,Trailer[0]);
#endif
		return 0;
		}

#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsBoardOk] BoardOK DONE\n");
#endif
	return (NBytes); // all size of the board

}

dataTypeSize_t EventBuilder::IsBoardOk(void *v,int MaxN)
	{
	// take ownership of myStream (*v)
	dataType myStream(MaxN,v);
	dataTypeSize_t R= IsBoardOk(myStream);
	// release ownership of myStream
	myStream.release();
	return R;
	}

dataTypeSize_t EventBuilder::IsEventOk(dataType &x){
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsEventOk] START\n");
#endif
	char *ptr=(char*)x.data();
	vector<WORD> myHead=StreamToWord(x.data(),WORDSIZE*EventHeaderWords()); // read the first two WORDS
	dataType H;EventHeader(H);
	dataType T;EventTrailer(T);
	
	vector<WORD> Header=StreamToWord( H );
	vector<WORD> Trailer=StreamToWord( T );

	// check header
	if( myHead.size() <EventHeaderWords() ) 
		{
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsEventOk] Head size is Wrong: %u == %u\n",myHead.size(), EventHeaderWords());
#endif
		return 0;
		}
	if( myHead[0] != Header[0] ) {
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsEventOk] Head is Wrong\n");
#endif
		return 0;
		}
	// header is fine
	WORD nBoard   = myHead[EventNboardsPos()];
	WORD eventSize= myHead[EventSizePos()];
	WORD eventNum = myHead[EventEnumPos()];

	dataTypeSize_t leftsize=x.size() - WORDSIZE*EventHeaderWords();
	ptr += WORDSIZE*EventHeaderWords() ;
	for(WORD iBoard = 0 ; iBoard < nBoard ;iBoard++)
		{
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsEventOk] Checking Board %u/%u\n",iBoard,nBoard);
#endif
		dataTypeSize_t readByte=IsBoardOk(ptr, leftsize);
		if (readByte==0) {
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsEventOk] A Board is not Ok\n");
#endif
			return 0;
			}
		leftsize -= readByte;
		ptr += readByte;
		}
	vector<WORD> myTrail=StreamToWord( ptr , WORDSIZE ) ;
	ptr += WORDSIZE;
	if ( myTrail[0] != Trailer[0] )  {
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsEventOk] Trail is Wrong\n");
#endif
		return 0;
		}
	//mismatch in size
	if (eventSize != (WORD)(ptr -(char*)x.data()) ) 
		{
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsEventOk] Size Match is Wrong\n");
#endif
		return 0;
		}
	return ptr - (char*)x.data();
#ifdef EB_DEBUG
	printf("[EventBuilder]::[IsEventOk] DONE");
#endif
} 

WORD 	EventBuilder::ReadRunNumFromSpill(dataType &x)
{
	WORD *spillNumPtr=(WORD*)x.data() + 1;
	return *spillNumPtr;
}

WORD 	EventBuilder::ReadSpillNum(dataType &x)
{
	WORD *spillNumPtr=(WORD*)x.data() + 2;
	return *spillNumPtr;
}
WORD 	EventBuilder::ReadEventNboards(dataType &x)
{
	WORD *spillNumPtr=(WORD*)x.data() + EventNboardsPos();
	return *spillNumPtr;
}
WORD 	EventBuilder::ReadEventNumber(dataType &x)
{
	WORD *eventNumPtr=(WORD*)x.data() + 1;
	return *eventNumPtr;
}
WORD 	EventBuilder::ReadSpillNevents(dataType &x)
{
	WORD *spillNeventsPtr=(WORD*)x.data() + 4;
	return *spillNeventsPtr;
}

// ---- EVENT BUILDER NON STATIC -----
void EventBuilder::Config(Configurator &c){
#ifndef NO_XML
        xmlNode *eb_node = NULL;
        //locate EventBuilder Node
        for (eb_node = c.root_element->children; eb_node ; eb_node = eb_node->next)
        {
                if (eb_node->type == XML_ELEMENT_NODE &&
                                xmlStrEqual (eb_node->name, xmlCharStrdup ("EventBuilder"))  )
                        break;
        }
        if ( eb_node== NULL ) throw  config_exception();
	dumpEvent_=Configurator::GetInt(getElementContent(c, "dumpEvent" , eb_node) );
	sendEvent_=Configurator::GetInt(getElementContent(c, "sendEvent" , eb_node) );
	recvEvent_=Configurator::GetInt(getElementContent(c, "recvEvent" , eb_node) );
	//dump_->SetFileName(getElementContent(c, "dumpFileName" , eb_node) );
	dirName_=getElementContent(c, "dumpDirName" , eb_node) ;
	bool dumpCompress=Configurator::GetInt(getElementContent(c, "dumpCompress" , eb_node) );
	dump_->SetCompress(dumpCompress);
	dump_->SetBinary();
	postBuiltCmd_="";
	postBuiltCmd_=getElementContent(c, "postBuiltCmd" , eb_node) ;

	ostringstream s;
	s<<"[EventBuilder]::[Config]::[INFO] DumpEvent="<<dumpEvent_;
	Log(s.str(),1);
	s.str("");
	s<<"[EventBuilder]::[Config]::[INFO] RecvEvent="<<recvEvent_;
	Log(s.str(),1);
	s.str("");
	s<<"[EventBuilder]::[Config]::[INFO] SendEvent="<<sendEvent_;
	Log(s.str(),1);
#else
	printf("[EventBuilder]::[Config] NO_XML Action Forbid\n");
	throw config_exception();

#endif
}

void EventBuilder::Init(){
	if(dumpEvent_)dump_->Init();
}

void EventBuilder::Clear(){
	if(dumpEvent_)dump_->Clear();
}
//
// SPILL 
void EventBuilder::OpenSpill()
{
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] OpenSpill",3);
#endif
	if (isSpillOpen_) CloseSpill(); 
	lastEvent_.spillNum_ ++;
	lastEvent_.eventInSpill_=1;
	if (dumpEvent_ && !recvEvent_) 
	{ // open dumping file
		dump_->Close();
		string newFileName= dirName_ + "/" + to_string((unsigned long long) lastEvent_.runNum_) + "/" + to_string((unsigned long long)lastEvent_.spillNum_);
		if (dump_->GetCompress() )  newFileName += ".raw.gz";
		else newFileName +=".raw";
		dump_->SetFileName( newFileName );	
		dump_->Init();
		Log("[EventBuilder]::[OpenSpill] Open file name:" + newFileName,1) ;
	}
	isSpillOpen_=true;
	SpillHeader(mySpill_);  // add Header SPLH
	mySpill_.append( (void *)& lastEvent_.runNum_,WORDSIZE); // RUN Num
	mySpill_.append( (void *)& lastEvent_.spillNum_,WORDSIZE); // Spill Num
	WORD zero=0;
	mySpill_.append( (void*)&zero, WORDSIZE); // spill Size
	mySpill_.append( (void*)&zero, WORDSIZE); // n.event in Spill
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] OpenSpill Done",3);
#endif
	
}

Command EventBuilder::CloseSpill()
{
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Close Spill",3);
#endif
	Command myCmd; myCmd.cmd=NOP;
	isSpillOpen_=false;	
	dataType  spillT;  SpillTrailer(spillT);
	mySpill_.append(spillT);
	// ack the position and the n.of.events
	WORD *nEvents=((WORD*)mySpill_.data()) + SpillNeventPos();
	WORD *spillSize=((WORD*)mySpill_.data()) + SpillSizePos();
	(*spillSize)= (WORD)mySpill_.size();
	(*nEvents)  = (WORD)lastEvent_.eventInSpill_-1; // 


	if( dumpEvent_ && !recvEvent_) 
	{
		Log("[EventBuilder]::[CloseSpill] File Closed",2) ;
		Dump(mySpill_);
		dump_->Close();
	}
	if (recvEvent_) { 
		Log("[EventBuilder]::[CloseSpill] File In Recv Mode",2) ;
		// For recv event, Close Spill should do nothing.
		// MergeSpills will dump in case
		//

		//WORD spillNum=ReadSpillNum(mySpill_);
		// --- if ( spills_.find(spillNum) == spills_.end() ) 
		// --- 	{
		// --- 	spills_[spillNum] = pair<int,dataType>(1, mySpill_ ); // spills_[] take ownership of the structuer -- copy constructor + release
		// --- 	mySpill_.release();
		// --- 	}
		// --- else MergeSpills(spills_[spillNum].second,mySpill_);
	} 
	if (sendEvent_) {//TODO -- also do the merging if recv
		// --- Instruct Daemon to send them through the connection manager
		Log("[EventBuilder]::[CloseSpill] File In Send Mode",2) ;
		myCmd.cmd=SEND;
		dataType myMex;
		myMex.append((void*)"DATA\0",5);
		myMex.append(mySpill_);
		myCmd.data=myMex.data();
		myCmd.N=myMex.size();
		myMex.release();
		} 
	mySpill_.clear();
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Close Spill - Done",3);
#endif
	return myCmd;
}

void EventBuilder::AddEventToSpill(dataType &event){
	if (!isSpillOpen_) return; // throw exception TODO
	// find the N.Of.Event in the actual RUn
	if (mySpill_.size() < WORDSIZE*4)  return; //throw exception TODO
	
	lastEvent_.eventInSpill_++;
	// this are updated in the CloseSpill -- should we keep them consistent
	//WORD *nEventsPtr=((WORD*)mySpill_.data() +3 );
	//WORD nEvents= *nEventsPtr;
	//nEvents+=1;
	//(*nEventsPtr)=nEvents;
	
	mySpill_.append(event);
	return;
}
int EventBuilder::MergeSpills(dataType &spill1,dataType &spill2 ){  // 0 ok
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Merge Spill 2 static",3);
	{
	ostringstream s; s<<"[EventBuilder]::[DEBUG] Merge Spill 2 static: size1"<<spill1.size()<<";"<<spill2.size() <<" ; "<<spill1.data() <<"!=NULL ;" << spill2.data() <<" !=NULL" ;
	Log(s.str(),3);
	}
#endif

	// check header	
#ifdef EB_DEBUG
	{
	WORD H1= *((WORD*) spill1.data() );
	WORD H2= *((WORD*) spill2.data() );
	dataType H1d,H2d;
	WordToStream(H1d,H1); H1d.append( (void*)"\0",1);
	WordToStream(H2d,H2); H2d.append( (void*)"\0",1);
	ostringstream s; s<<"[EventBuilder]::[DEBUG] Merge Spill 2 static: Headers"<< (char*)H1d.data() <<";"<< (char*) H2d.data() ;
	Log(s.str(),3);
	}
#endif
		
	// --- can't merge inplace two events
	WORD runNum1 = ReadRunNumFromSpill(spill1);
	WORD runNum2 = ReadRunNumFromSpill(spill2);

	WORD spillNum1 = ReadSpillNum(spill1);
	WORD spillNum2 = ReadSpillNum(spill2);

	WORD spillNevents1 = ReadSpillNevents(spill1);
	WORD spillNevents2 = ReadSpillNevents(spill2);

	WORD zero=0; //spillSize

#ifdef EB_DEBUG
	{
	ostringstream s; s<<"[EventBuilder]::[DEBUG] Merge Spill 2 static: Merging runs"<<runNum1<<"=="<<runNum2;
	Log(s.str(),3);
	s<<"[EventBuilder]::[DEBUG] Merge Spill 2 static: Merging spillN"<<spillNum1<<"=="<<spillNum2;
	Log(s.str(),3);
	s.str() ="";
	 s<<"[EventBuilder]::[DEBUG] Merge Spill 2 static: Merging spill Nevents="<<spillNevents1<<"=="<<spillNevents2;
	Log(s.str(),3);
	}
#endif

	if (runNum1 != runNum2) { Log("[EventBuilder]::[MergeSpills]::[ERROR] RunNumber does not match",1); return 1; } // 
	if (spillNum1 != spillNum2){ Log("[EventBuilder]::[MergeSpills]::[ERROR] Spill numbers does not match",1);return 1;} 
	if (spillNevents1 != spillNevents2){Log("[EventBuilder]::[MergeSpills]::[WARNING] Event in spills different. Trying ignoring last.",1); } // return 1; } // assume lasts are wrong
	if (  abs(int64_t(spillNevents1) - int64_t(spillNevents2)) >1 ) { Log("[EventBuilder]::[MergeSpills]::[WARNING] Event in spills different. Ignoring spill.",1); return 1;}
	dataType oldSpill(spill1.size(),spill1.data());	
	spill1.release();spill1.clear();

	dataType H;SpillHeader(H);
	dataType T;SpillTrailer(T);

	spill1.append(H);
	spill1.append((void*)&runNum1,WORDSIZE); // 
	spill1.append((void*)&spillNum1,WORDSIZE); // 
	spill1.append( (void*)&zero, WORDSIZE); // spillSize
	spill1.append((void*)&spillNevents1,WORDSIZE); // will be updated

#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Merge Spill 2 static: Headers Added",3);
#endif

	char* ptr1=(char*)oldSpill.data();
	char* ptr2=(char*)spill2  .data();
	long left1=oldSpill.size() - WORDSIZE*SpillHeaderWords();
	long left2=spill2.size() - WORDSIZE*SpillHeaderWords();
	ptr1+= WORDSIZE*SpillHeaderWords();
	ptr2+= WORDSIZE*SpillHeaderWords();

	long dTimeFirstEvent=0;
	int skippedEvents=0;
	WORD merged=0;
	for(unsigned long long iEvent=0;iEvent< spillNevents1 && iEvent< spillNevents2 ;iEvent++)
	       {
#ifdef EB_DEBUG
	ostringstream s; s<<"[EventBuilder]::[DEBUG] Merge Spill 2 static: Merge iEvent="<<iEvent;
	Log(s.str(),3);
	s.str()="";
	s<<"[EventBuilder]::[DEBUG] Merge Spill 2 static: Event Headers: "<< ptr1 << " : "<<ptr2 ;
	Log(s.str(),3);
#endif
		long eventSize1= IsEventOk(ptr1,left1);
		long eventSize2= IsEventOk(ptr2,left2);
#ifdef EB_DEBUG
	s.str()="";
	s<<"[EventBuilder]::[DEBUG] Merge Spill 2 static: EventSize1"<<eventSize1<<"!=0  eventSize2="<<eventSize2<<"!=0"<< " Left="<< left1 <<" : "<<left2;
	Log(s.str(),3);
#endif
		if (eventSize1 == 0 || eventSize2 == 0 ) return 2;
		//---- Unire i due eventi
		//check Time here;
		// assuming first board is a time board
		// check that first board is time _TIME_
		WORD tB=(WORD)_TIME_;
		WORD bId1 = * (WORD*)(ptr1+ (EventTimePos()-2)*WORDSIZE );
		WORD bId2 = * (WORD*)(ptr2+ (EventTimePos()-2)*WORDSIZE );
		if ( (bId1 &GetBoardTypeIdBitMask () ) >> GetBoardTypeShift() != tB )  
			{
			ostringstream s; s<<"[EventBuilder]::[MergeSpill]::[ERROR] Time Board not the first board: bId1= "<<bId1
					  <<" bId1&bitMask= "<<(bId1 &GetBoardTypeIdBitMask ()) << " lowbit= "<< ((bId1 &GetBoardTypeIdBitMask () ) >> GetBoardTypeShift() )<< " == tB=" << tB ; 
			Log(s.str(),1); 
			}
		if ( (bId2 &GetBoardTypeIdBitMask () ) >> GetBoardTypeShift() != tB )  
			{
			ostringstream s; s<<"[EventBuilder]::[MergeSpill]::[ERROR] Time Board not the first board: bId2="<<bId2
					  <<"bId2&bitMask="<<(bId2 &GetBoardTypeIdBitMask ()) << " lowbit="<<((bId2 &GetBoardTypeIdBitMask () ) >> GetBoardTypeShift () )<< " == tB=" << tB ; 
			Log(s.str(),1); 
			} 
		uint64_t time1 = *( (uint64_t*) (ptr1 + EventTimePos()*WORDSIZE)   );  // carefull to parenthesis
		uint64_t time2 = *( (uint64_t*) (ptr2 + EventTimePos()*WORDSIZE)   );  // carefull to parenthesis
		int skipMe=0;
		if (iEvent==0 ) dTimeFirstEvent=int64_t(time1)-int64_t(time2);
		// assume sigma=1*sqrt(2), cutting on 20
		else if ( abs(int64_t(time1)-int64_t(time2) -dTimeFirstEvent )>50 )
			{
			ostringstream s2;s2<<"[EventBuilder]::[MergeSpill]::[Warning] bad spill for time";
			// this is a bad Spill
			Log(s2.str(),1);
			skipMe++;
			skippedEvents++;
			if (skippedEvents >20) 
				{
				ostringstream s2;s2<<"[EventBuilder]::[MergeSpill]::[Warning] Too many time wrong: ignoring spill";
				return 3;
				}
			}
#ifdef TIME_DEBUG
		{ // this code may slow down the code, as well as produce large log files
		ostringstream s2;
			s2<<"[EventBuilder]::[TIMEINFO] "<<spillNum1<<" "<<iEvent<<" "<< time1<<" "<<time2;
		Log(s2.str(),2);
		}
#endif

		dataType event1(eventSize1,ptr1);
		dataType event2(eventSize2,ptr2);
		dataType myEvent;MergeEventStream(myEvent,event1,event2);
		event1.release();
		event2.release();
		if(!skipMe){
			spill1.append(myEvent);
			merged++;
			}
		//----
		ptr1+= eventSize1;
		ptr2+= eventSize2;
		left1-=eventSize1;
		left2-=eventSize2;
	       }	
	
	spill1.append(T); 
	// update Spill Size
	WORD *spillSizePtr= ((WORD*) spill1.data() )+ SpillSizePos();
	(*spillSizePtr)=(WORD)spill1.size();

	WORD *spillNevtPtr= ((WORD*) spill1.data() )+ SpillNeventPos();
	(*spillNevtPtr)=(WORD)merged;
	
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Merge Spill 2 static Done",3);
#endif
	return 0;
}

void EventBuilder::MergeSpills(dataType &spill2 ) {
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Merge Spill - 1",3);
	ostringstream s; s<<"[EventBuilder]::[DEBUG] Merge Spill: Event Already merged="<<merged_<<" lastBadSpill Was "<<lastBadSpill_ <<" SpillN is "<<ReadSpillNum(spill2);
	Log(s.str(),3);
#endif
	WORD spillNum=ReadSpillNum(spill2); 

		if ( merged_ == 0 )  // spill structure is empty
			{
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Merge Spill - 1: NO MERGED EVENTS: CHECK IF SPILL IS MARKED AS BAD",3);
#endif
			// TODO -- check spill consistencies to call this function. Check if spill2 is better
			if (lastBadSpill_ == ReadSpillNum(spill2) ) return;
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Merge Spill - 1: COPY",3);
#endif
			mySpill_.copy(spill2);
			++merged_;
			}
		else 
			{
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Merge Spill - 1: Merge",3);
#endif
			int badSpill=MergeSpills(mySpill_,spill2);
			++merged_;
			if(badSpill){
					// erase spill structure -- W/O dumping
					// TODO -- check spill consistencies to call this function. Check if spill2 is better
					int sn1=ReadSpillNum(mySpill_);
					lastBadSpill_=sn1;
					++badSpillsInThisRun_;
					merged_=0;
					mySpill_.clear();
					}
			}
	if (!recvEvent_ ) return ;

	if ( merged_ >= recvEvent_)  // dump for recvEvent
		{
		WORD myRunNum=ReadRunNumFromSpill(spill2);
		if (mySpill_.size()>4) goodEventsInThisRun_+=ReadSpillNevents(mySpill_);
		string myDir=dirName_ + "/" + to_string((unsigned long long) myRunNum) + "/";
		system( ("mkdir -p " + myDir ) .c_str() );
		string newFileName= dirName_ + "/" + to_string((unsigned long long) myRunNum) + "/" + to_string((unsigned long long)spillNum);
		if (dump_->GetCompress() )  newFileName += ".raw.gz";
		else newFileName +=".raw";
		dump_->SetFileName( newFileName );	
		dump_->Init();
		Log("[EventBuilder]::[MergeSpill] Open file name:" + newFileName,1) ;
		Dump(mySpill_);
		dump_->Close();
#ifdef EB_DEBUG
	Log("[EventBuilder]::[DEBUG] Merge Spill - 1 Going To Clear",3);
		usleep(1000);
#endif
		mySpill_.clear();
		merged_=0;
		// Spill is completed and written to file	
		string myCmd= postBuiltCmd_;
		// change the cmd strings
		FindAndReplace(myCmd,"\%\%RUN\%\%", to_string((unsigned long long)myRunNum));	
		FindAndReplace(myCmd,"\%\%SPILL\%\%", to_string((unsigned long long)spillNum));	
		FindAndReplace(myCmd,"\%\%FILE\%\%", newFileName);	
		FindAndReplace(myCmd,"\%\%RAWDIR\%\%", dirName_);	

		pid_t childpid=Fork();
		if (childpid == 0 ) // child
			{
			Log("[EventBuilder]::[MergeSpill]::[DEBUG] Executing command:"+myCmd,3);
			system(myCmd.c_str());
			_exit(0);
			}
		else if (childpid <0 ) {
			Log("[EventBuilder]::[MergeSpill]::[ERROR] Cannot spawn a process. Unpack not done.",1);
			ostringstream s;
			s<<"[EventBuilder]::[MergeSpill]::[ERROR] Currently running "<<GetN()<<" children processes";
			Log(s.str(),1);
			}
		// do nothing in case of parent
		}
#ifdef EB_DEBUG

	Log("[EventBuilder]::[DEBUG] Merge Spill - 1 Done",3);
#endif
	return;
} 

void EventBuilder::SetRunNum(WORD x)
{
//runNum_=x;
lastEvent_.runNum_=x;
goodEventsInThisRun_=0;
lastBadSpill_=0;
badSpillsInThisRun_=0;
if (dumpEvent_ || recvEvent_)  // POSIX
  system(  ("mkdir -p "+dirName_+ to_string((unsigned long long)x) ).c_str() );
}


void EventBuilder::OpenEvent( dataType &R , WORD nBoard){
	if (R.size() >0 ) {
			Log("[EventBuilder]::[OpenEvent] Return Size is more than 0",1);
			throw logic_exception();
			}
        // Construt the event
        EventBuilder::EventHeader(R);

        EventBuilder::WordToStream(R, lastEvent_.eventInSpill_ );
        WORD zero=0;
        EventBuilder::WordToStream(R,zero); //eventSize in Byte
        EventBuilder::WordToStream(R,nBoard); // nBoard
	return;
}

void EventBuilder::CloseEvent( dataType &R){
	EventBuilder::EventTrailer(R);
        // N.Of Byte of the stream
        WORD* EventSizePtr = ((WORD*)R.data() ) + EventSizePos();
        (*EventSizePtr) = (WORD)R.size();
	return;

}

WORD EventBuilder::GetBoardTypeId(dataType &R){
	WORD myMergedId= *((WORD*)R.data() + BoardIdPos());
	WORD myResult= ( myMergedId & GetBoardTypeIdBitMask())>>GetBoardTypeShift();
	return myResult;
}

WORD EventBuilder::GetBoardBoardId(dataType &R){
	WORD myMergedId= *((WORD*)R.data() + BoardIdPos());
	WORD myResult= ( myMergedId & GetBoardIdBitMask())>>GetBoardIdShift();
	return myResult;
}

WORD EventBuilder::GetBoardCrateId(dataType &R){
	WORD myMergedId= *((WORD*)R.data() + BoardIdPos());
	WORD myResult= ( myMergedId & GetCrateIdBitMask())>>GetCrateIdShift();
	return myResult;
}

int EventBuilder::FindAndReplace(string &myString,string find, string replace)
{
	size_t f = myString.find(find);
	if (f == string::npos) return 1;
	myString=myString.replace(f, find.length(), replace );
return 0;
}
