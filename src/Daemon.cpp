#include "interface/Daemon.hpp"
#include "interface/Utility.hpp"

//#define DAEMON_DEBUG

// --- Constructor 
Daemon::Daemon(){
	// get the pid of the current process
	pid_=getpid();
	//  Construct objects
	configurator_=new Configurator();
	eventBuilder_=new EventBuilder();
	hwManager_=new HwManager();
	connectionManager_=new ConnectionManager();
	myStatus_=START;
	myPausedFlag_=false;
	gettimeofday(&lastSentStatusMessageTime_,NULL);
	gettimeofday(&start_time,NULL);
	iLoop=0;
	waitForDR_=0;
	noEB_=0;
	spillSignalsDisabled_=0;
	testEnableDuringBeam_=-1;
	testEnableSequence_=vector<TRG_t>();
	srand((unsigned)time(NULL));
}

Daemon::~Daemon(){
	if(eventBuilder_) delete eventBuilder_;
	if(connectionManager_) delete connectionManager_;
	if(hwManager_) delete hwManager_;
	if(configurator_) delete configurator_;
	eventBuilder_ = NULL;
	connectionManager_=NULL;
	hwManager_=NULL;
	configurator_=NULL;
}


int Daemon::Init(string configFileName){
	try{
		// Set Configurator ; and Init it
		configurator_->xmlFileName=configFileName;
		configurator_->Init();
		//these is used for RC & EB (number of DR to be waited)
		waitForDR_=Configurator::GetInt(Configurable::getElementContent(*configurator_,"waitForDR",configurator_->root_element) ); // move to Config
		//these 2 are only used for RC
		noEB_=Configurator::GetInt(Configurable::getElementContent(*configurator_,"noEB",configurator_->root_element) ); // move to Config
		spillSignalsDisabled_=Configurator::GetInt(Configurable::getElementContent(*configurator_,"spillSignalsDisabled",configurator_->root_element) ); // move to Config
		testEnableDuringBeam_=Configurator::GetInt(Configurable::getElementContent(*configurator_,"testEnableDuringBeam",configurator_->root_element) ); // move to Config
		string content = Configurable::getElementContent (*configurator_, "testEnableSequence",configurator_->root_element) ;
		istringstream ss(content);
		string token;
		while(std::getline(ss, token, ',')) 
		  testEnableSequence_.push_back(TRG_t(Configurator::GetInt(token)));
		
		ostringstream s; s<<"[Daemon]::[Init] Wait For DR: "<< waitForDR_ ;
		Log(s.str(),1);
		s.str(""); s<<"[Daemon]::[Init] Use EB: "<< !noEB_;
		Log(s.str(),1);
		s.str(""); s<<"[Daemon]::[Init] Spill Signals: "<< !spillSignalsDisabled_;
		Log(s.str(),1);
		s.str(""); s<<"[Daemon]::[Init] Test Enable During Beam: "<< testEnableDuringBeam_;
		Log(s.str(),1);
		s.str(""); s<<"[Daemon]::[Init] Test Enable sequence:";
		for (unsigned int it=0; it<testEnableSequence_.size();++it)
		  s<< " " << testEnableSequence_[it];
		Log(s.str(),1);

		//		printf("%s\n",s.str().c_str());
		// Configure Everything else
		eventBuilder_		->Config(*configurator_);
		hwManager_		->Config(*configurator_);
		connectionManager_	->Config(*configurator_);
		// Init Everything
		eventBuilder_->Init();
		hwManager_->Init();
		connectionManager_->Init();
		ConfigLogConnManager(connectionManager_,StatusSck);

		return 0;
	} catch( std::exception &e) 
		{
		printf("Exception: %s\n",e.what());
		throw e;
		};
}


void Daemon::Clear()
{
	if (configurator_) { configurator_->Clear(); delete configurator_; }
	if (eventBuilder_) {eventBuilder_->Clear(); delete eventBuilder_; }
	if (hwManager_) { hwManager_->Clear(); delete hwManager_; }
	if (connectionManager_) { connectionManager_->Clear(); ConfigLogConnManager(NULL,-999); delete connectionManager_;}
}


Command Daemon::ParseData(dataType &mex)
{
	Command myCmd;
	int N=mex.size();
	// SEND
	if (N >=5  and !strcmp( (char*) mex.data(), "SEND")  )
		//	((char*)mex.data() ) [0]== 'S' and
		//	((char*)mex.data() ) [1]== 'E' and
		//	((char*)mex.data() ) [2]== 'N' and
		//	((char*)mex.data() ) [3]== 'D' and
		//	((char*)mex.data() ) [4]== '\0'  
		{
		//mex.erase(dataTypeSize_t(0),dataTypeSize_t(5));
		mex.erase(0,5);
		myCmd.cmd=SEND;
		myCmd.data = mex.data();
		myCmd.N    = mex.size();
		mex.release();
		}
	else if (N >=5  and !strcmp( (char*) mex.data(), "RECV")  )
		{
		mex.erase(0,5);
		myCmd.cmd=RECV;
		myCmd.data = mex.data();
		myCmd.N    = mex.size();
		mex.release();
		}
	else if (N >=5  and !strcmp( (char*) mex.data(), "DATA")  )
		{
		mex.erase(0,5);
		myCmd.cmd=DATA;
		myCmd.data = mex.data();
		myCmd.N    = mex.size();
		mex.release();
		}
	else if (N >=4  and !strcmp( (char*) mex.data(), "NOP")  )
		myCmd.cmd=NOP;
	else if (N >=4  and !strcmp( (char*) mex.data(), "WWE")  )
		myCmd.cmd=WWE;
	else if (N >=3  and !strcmp( (char*) mex.data(), "WE")  )
		myCmd.cmd=WE;
	else if (N >=3  and !strcmp( (char*) mex.data(), "EE")  )
		myCmd.cmd=EE;
	else if (N >=4  and !strcmp( (char*) mex.data(), "WBT")  )
		myCmd.cmd=WBT;
	else if (N >=4  and !strcmp( (char*) mex.data(), "WBE")  )
		myCmd.cmd=WBE;
	else if (N >=4  and !strcmp( (char*) mex.data(), "EBT")  )
		myCmd.cmd=EBT;
	//	else if (N >=7  and !strcmp( (char*) mex.data(), "STATUS")  ) // move in the not null terminated part TODO
	//		{
	//		mex.erase(0,7);
	//		myCmd.cmd=STATUS;
	//		myCmd.data = mex.data();
	//		myCmd.N    = mex.size();
	//		mex.release();
	//		}
	else if (N >=9  and !strcmp( (char*) mex.data(), "STARTRUN")  )
		{
		mex.erase(0,9);
		myCmd.cmd=STARTRUN;
		myCmd.data = mex.data();
		myCmd.N    = mex.size();
		mex.release();
		}
	else if (N >=11  and !strcmp( (char*) mex.data(), "SPILLCOMPL")  )
		myCmd.cmd=SPILLCOMPL;
	else if (N >=14  and !strcmp( (char*) mex.data(), "EB_SPILLCOMPL")  )
		myCmd.cmd=EB_SPILLCOMPLETED;
	else if (N >=9  and !strcmp( (char*) mex.data(), "DR_READY")  )
		myCmd.cmd=DR_READY;
	else if (N >=7  and !strcmp( (char*) mex.data(), "ENDRUN")  )
		myCmd.cmd=ENDRUN;
	else if (N >=4  and !strcmp( (char*) mex.data(), "DIE")  )
		myCmd.cmd=DIE;
	else if (N >=9  and !strcmp( (char*) mex.data(), "RECONFIG")  )
		myCmd.cmd=RECONFIG;
	else if (N >=6  and !strcmp( (char*) mex.data(), "ERROR")  )
		{
		//It doesn't matter wherever you are, if this happens FSM are de-sync, so move immediately to status error
		  Log("[Daemon]::[ParseCommand]::[INFO]::Received ERROR COMMAND!",1);
		  myCmd.cmd=ERRORCMD;
		  MoveToStatus(ERROR);
		}
	// GUI CMD are not NULL Terminated
	else 	{ // GUI --- I'M changing the mex

		dataType mex2;
		mex2.copy(mex.data(),mex.size()) ;
		mex2.append((void*)"\0",1);
		Utility::SpaceToNull(mex2.size(),mex2.data(),false); // true=only the first
#ifdef DAEMON_DEBUG
		ostringstream s; s<<"[Daemon]::[ParseCommand]::[DEBUG] GUI Mex: '"<< (char*)mex2.data()<<"'";
		Log(s.str(),3);
#endif
		ostringstream s; s<<"[Daemon]::[ParseCommand]::[DEBUG] GUI Mex: '"<< (char*)mex2.data()<<"'";
		Log(s.str(),3);

		if (N >=4  and !strcmp( (char*) mex2.data(), "NOP")  )
			myCmd.cmd=NOP;
		else if (N >=4  and !strcmp( (char*) mex2.data(), "WWE")  )
			myCmd.cmd=WWE;
		else if (N >=3  and !strcmp( (char*) mex2.data(), "WE")  )
			myCmd.cmd=WE;
		else if (N >=3  and !strcmp( (char*) mex2.data(), "EE")  )
			myCmd.cmd=EE;
		else if (N >=9  and !strcmp( (char*) mex2.data(), "STARTRUN")  )
			{
			mex2.erase(0,9);
			myCmd.cmd=STARTRUN;
			myCmd.data = mex.data();
			myCmd.N    = mex.size();
			mex2.release();
			}
		else if (N >=11  and !strcmp( (char*) mex2.data(), "SPILLCOMPL")  )
			myCmd.cmd=SPILLCOMPL;
		else if (N >=14  and !strcmp( (char*) mex2.data(), "EB_SPILLCOMPL")  )
			myCmd.cmd=EB_SPILLCOMPLETED;
		else if (N >=10  and !strcmp( (char*) mex2.data(), "DR_READY")  )
			myCmd.cmd=DR_READY;
		else if (N >=7  and !strcmp( (char*) mex2.data(), "ENDRUN")  )
			myCmd.cmd=ENDRUN;
		else if (N >=4  and !strcmp( (char*) mex2.data(), "DIE")  )
			myCmd.cmd=DIE;
		// GUI CMD are not NULL Terminated
		else if (N >=12  and !strcmp( (char*) mex2.data(), "GUI_STARTRUN")  )
		   {
		   mex2.erase(0,13);
		   myCmd.cmd=GUI_STARTRUN;
		   //Utility::SpaceToNull(mex2.size(),mex2.data() ) ;
		   myCmd.data = mex2.data();
		   myCmd.N    = mex2.size();
		   mex2.release();
		   }
		else if (N >=12  and !strcmp( (char*) mex2.data(), "GUI_PAUSERUN")  )
		   {
		   myCmd.cmd=GUI_PAUSERUN;
		   }
		else if (N >=11  and !strcmp( (char*) mex2.data(), "GUI_STOPRUN")  )
		   {
		   myCmd.cmd=GUI_STOPRUN;
		   }
		else if (N >=14  and !strcmp( (char*) mex2.data(), "GUI_RESTARTRUN")  )
		   {
		   myCmd.cmd=GUI_RESTARTRUN;
		   }
		else if (N >=7  and !strcmp( (char*) mex2.data(), "GUI_DIE")  )
		   {
		   myCmd.cmd=GUI_DIE;
		   }
		else if (N >=12  and !strcmp( (char*) mex2.data(), "GUI_RECONFIG")  )
		   {
		   myCmd.cmd=GUI_RECONFIG;
		   }
		if (myCmd.data != NULL)mex2.release();
		} // ENDGUI

	if(myCmd.data !=NULL ) mex.release();
	return myCmd;
}



void Daemon::MoveToStatus(STATUS_t newStatus){
	// -- dataType myMex;
	// -- myMex.append((void*)"STATUS ",7);
	// -- WORD myStatus=(WORD)newStatus;
	// -- myMex.append((void*)&myStatus,WORDSIZE);
	// -- connectionManager_->Send(myMex,StatusSck);

	ostringstream s;
	s << "[Daemon]::[DEBUG]::Moving to status " << newStatus;
#ifndef FSM_DEBUG
	if (newStatus<CLEARBUSY || newStatus>READ)
	  Log(s.str(),3);
#else
	  Log(s.str(),3);
#endif
	std::cout << s.str() << std::endl;
	STATUS_t oldStatus = myStatus_;
	myStatus_=newStatus;
	myPausedFlag_=false;
	if (!((oldStatus==CLEARBUSY && newStatus==WAITTRIG) ||	\
	      (oldStatus==WAITTRIG && newStatus==READ) ||	\
	      (oldStatus==READ && newStatus==CLEARBUSY) )) {
	  SendStatus(oldStatus,newStatus); //Send status to GUI (formatted correctly)
	  }
}

void Daemon::SendStatus(STATUS_t oldStatus, STATUS_t newStatus){
	dataType myMex;
	myMex.append((void*)"STATUS ",7);
	char mybuffer[255];
	int n=0;
	myMex.append((void*)"statuscode=",11);
	n = snprintf(mybuffer,255,"%u ",newStatus);
	myMex.append((void*)mybuffer,n);
	WORD runnr = 0;
	WORD spillnr = 0;
	WORD evinspill = 0;
	if (eventBuilder_){
	  runnr = eventBuilder_->GetEventId().runNum_;
	  spillnr = eventBuilder_->GetEventId().spillNum_;
	  if (eventBuilder_->GetEventId().eventInSpill_>0){
	    evinspill = eventBuilder_->GetEventId().eventInSpill_-1;
	    //evinspill-1 because lastEvent_ is number_of_events+1
	  }
	}
	myMex.append((void*)"runnumber=",10);
	n = snprintf(mybuffer,255,"%u ",runnr); //runnr
	myMex.append((void*)mybuffer,n);
	myMex.append((void*)"spillnumber=",12);
	n = snprintf(mybuffer,255,"%u ",spillnr); //spillnr
	myMex.append((void*)mybuffer,n);
	myMex.append((void*)"evinspill=",10);
	n = snprintf(mybuffer,255,"%u ",evinspill); 
	myMex.append((void*)mybuffer,n);
	myMex.append((void*)"paused=",7);
	if (myPausedFlag_) myMex.append((void*)"1",1);
	else myMex.append((void*)"0",1);
	connectionManager_->Send(myMex,StatusSck);
	gettimeofday(&lastSentStatusMessageTime_,NULL);
}

void Daemon::PublishStatusWithTimeInterval(){
  timeval tv;
  gettimeofday(&tv,NULL);
  long timediff = Utility::timevaldiff(&lastSentStatusMessageTime_,&tv); // in usec
  if (timediff>200000) SendStatus(myStatus_,myStatus_);
}

bool Daemon::IsOk(){return true;}
void Daemon::LogInit(Logger*l){
				LogUtility::LogInit(l);
				eventBuilder_->LogInit(l);
				hwManager_->LogInit(l);
				connectionManager_->LogInit(l);
				//configurator_->LogInit(l);
				};

int Daemon::Daetach(){

	pid_t pid=fork();
	if (pid >0 ){ // parent
		printf("[EventBuilderDaemon] Detaching process %d\n",pid);
		exit(0); //
		} 
	else if (pid== 0 ) { // child
		setsid(); // obtain a new group process
		// close all descriptors
		fflush(NULL);
		int i;
		for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
		i=open("/dev/null",O_RDWR); /* open stdin */
		dup(i); /* stdout */
		dup(i); /* stderr */
		}
	else {
		printf("[EventBuilderDaemon] Cannot Daemonize");
		return 1;
		}

	return 0;
}


void Daemon::ErrorStatus(){
	// if entered in this loop for the first time
	if ( !error_)
		{
		dataType errMex;
		errMex.append((void*)"ERROR\0\0",6);
		// send 3 error mex
		connectionManager_->Send(errMex,CmdSck);
		sleep(1);
		connectionManager_->Send(errMex,CmdSck);
		sleep(1);
		connectionManager_->Send(errMex,CmdSck);
		//Reset Members
		if(eventBuilder_)eventBuilder_->Reset();
		//hwManager_-> ???
		if(hwManager_)
			{
			hwManager_->Clear(); // call reset of all board
			//hwManager_->Config(*configurator_); //configure all boards -- ? 
			//hwManager_->Init(); // Init All Boards
			}
		}
	error_=true;
	//wait for instructions
	dataType myMex;
	if (connectionManager_->Recv(myMex) == 0 )
		{
		Command myCmd=ParseData(myMex)	;
		if (myCmd.cmd ==  ENDRUN ){
			error_=false;
			MoveToStatus(INITIALIZED);
			}
		}
	return;			
}

void Daemon::Reconfigure(){
  //Reset Members
  if(eventBuilder_) eventBuilder_->Reset();
  if(hwManager_) hwManager_->Clear(); 
  MoveToStatus(INITIALIZED);
  return;			
}
