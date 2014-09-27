#include "interface/HwManager.hpp"
#include "interface/CAEN_VX718.hpp"
#include "interface/TimeBoard.hpp"
#include "interface/EventBuilder.hpp" // boardId
//#include "interface/BoardConfig.hpp"


// -------------------  HW Manager ---------------
//
// --- Constructor
HwManager::HwManager(){

  trigBoard_.boardIndex_=-1;
  trigBoard_.boardHandle_=-1;

  ioControlBoard_.boardIndex_=-1;
  ioControlBoard_.boardHandle_=-1;

  controllerBoard_.boardIndex_=-1;
  controllerBoard_.boardHandle_=-1;

  digiBoard_.boardIndex_=-1;
  digiBoard_.boardHandle_=-1;

  crateId_=-1;
  //runControl_=false;
}
// --- Destructor
HwManager::~HwManager(){}
// --- Configure the HwManager
void HwManager::Config(Configurator &c){
	// take the configurator. Translate it into a Board config. Configure the board
	xmlNode *hw_node = NULL;	
	//locate Hardware Node
	for (hw_node = c.root_element->children; hw_node ; hw_node = hw_node->next)
	{
		if (hw_node->type == XML_ELEMENT_NODE &&
				xmlStrEqual (hw_node->name, xmlCharStrdup ("Hardware"))  )
			break;
	}
	if ( hw_node== NULL ) {
		printf("Hardware not found in configuration\n");
		throw  config_exception();
		}
	crateId_= Configurator::GetInt(getElementContent(c,"crateId",hw_node) );

	// locate each board node and extract info
	xmlNode *board_node = NULL;	
	for ( board_node=hw_node->children; board_node ; board_node = board_node->next)
	{
		if (  board_node->type == XML_ELEMENT_NODE 	
				&& xmlStrEqual (board_node->name, xmlCharStrdup ("board")) )
		{
		int ID=Configurator::GetInt(getElementContent(c, "ID" , board_node));
		Log("[2] Configuring Board ID="+ getElementContent(c, "ID" , board_node)+"type=" + getElementContent(c, "type" , board_node),2);
		//TODO -- construct the board -- if elif ... else throw exception 
		// keep the index where I'm constructing stuff
		int bIdx=hw_.size();
		if ( getElementContent(c,"type",board_node) == "CAEN_VX718" )
			{
			  // construct a CAEN_VX718 Board, and push it back
			  hw_.push_back( new CAEN_VX718() );
			}
		else if( getElementContent(c,"type",board_node) == "TIME")
			{
			  //constructing a TimeStamp Board
			  hw_.push_back( new TimeBoard() );
			}
		else
		  {
		    //UNKNOWN board get to the next node
		    Log("[HwManager]::[Config] No Board. Unknw board type",1);
		    continue; 
		  }
		// else if ( ... ==... ) ... TODO
		
		//Initialize Board Logger
		if (this->GetLogger() != NULL)
		  hw_[bIdx]->LogInit(this->GetLogger());

		// construct a board configurator and ask the board to configure itself
		BoardConfig bC;
		bC.Init(c);
		bC.SetBoardNode(board_node);
		hw_[bIdx]->Config(&bC);

		}
	}

	return;	
}

// --- Init
void HwManager::Init(){
  //Crate init
  if (hw_.size()>0 )
  	CrateInit();
  trigBoard_.boardIndex_=controllerBoard_.boardIndex_;

  // check that Trigger Board, if present, inheriths from TriggerBoard
  if ( trigBoard_.boardIndex_>=0) 
	  {
  	  TriggerBoard * tb= dynamic_cast<TriggerBoard*> (hw_[trigBoard_.boardIndex_]);
  	  if (tb==NULL) {
	  	Log("[HwManager]::[Init] Trigger Board does not inheriths from TriggerBoard class",1);
		throw config_exception();
  		}
	  }

  if ( ioControlBoard_.boardIndex_>=0) 
	  {
  	  IOControlBoard * iob= dynamic_cast<IOControlBoard*> (hw_[ioControlBoard_.boardIndex_]);
  	  if (iob==NULL) {
	  	Log("[HwManager]::[Init] IOControl Board does not inheriths from IOControlBoard class",1);
		throw config_exception();
  		}
	  }

  for(unsigned int i=0;i<hw_.size();i++)
	{
	  if (hw_[i]->GetType() != "CAEN_V1742" )
	    hw_[i]->SetHandle(controllerBoard_.boardHandle_);
	  else
	    hw_[i]->SetHandle(digiBoard_.boardHandle_);
		int R=hw_[i]->Init();
		if ( R )  
		  {
		    ostringstream s;
		    s << "[HwManager]::[ERROR]::Error configuring board " << hw_[i]->GetType() << " exit code " << R;
		    Log(s.str(),1);
		    throw hw_exception();
		  }
	}
}

// --- Crate Init
int HwManager::CrateInit()
{
  int status =0;

  for(unsigned int i=0;i<hw_.size();i++)
    {
      if ( hw_[i]->GetType() == "CAEN_V1742" )
	{
	  digiBoard_.boardIndex_=i;
	  break; //do not support >1 digitizer board for the moment...
	}
    }

  for(unsigned int i=0;i<hw_.size();i++)
    {
      if ( hw_[i]->GetType() == "CAEN_VX718" )
	{
	  controllerBoard_.boardIndex_=i;
	  break; 
	}
    }

  for(unsigned int i=0;i<hw_.size();i++)
    {
      if ( hw_[i]->GetType() == "CAEN_V513" || hw_[i]->GetType() == "CAEN_V262" )
	{
	  ioControlBoard_.boardIndex_=i;
	  break; 
	}
    }
 
  if (controllerBoard_.boardIndex_<0)
    {
      Log("[HwManager]::[ERROR]::Cannot find controller board",1);
      throw config_exception();
    }
  
  if (digiBoard_.boardIndex_<0)
    {
      CAEN_VX718::CAEN_VX718_Config_t* controllerConfig=((CAEN_VX718*)hw_[controllerBoard_.boardIndex_])->GetConfiguration();

      status |= CAENVME_Init(controllerConfig->boardType, controllerConfig->LinkType, controllerConfig->LinkNum, &controllerBoard_.boardHandle_);

      if (status)
	{
	  ostringstream s;
	  s << "[HwManager]::[ERROR]::VME Crate Type "<<controllerConfig->boardType<<" LinkType "<<controllerConfig->LinkType<<" DeviceNumber "<<controllerConfig->LinkNum << " cannot be initialized"  ;
	  Log(s.str(),1);
	  throw config_exception();
	}
      ostringstream s;
      s << "[HwManager]::[INFO]::VME Crate Type "<<controllerConfig->boardType<<" LinkType "<<controllerConfig->LinkType<<" DeviceNumber "<<controllerConfig->LinkNum << " initialized"  ;
      Log(s.str(),1);
    }
  else
    {
      //have digitizer
      // TODO
    }

  if (controllerBoard_.boardHandle_<0)
    {
      Log("[HwManager]::[ERROR]::VME Crate Controller Handle is wrong",1);
      throw config_exception();
    }

  status |= CAENVME_SystemReset(controllerBoard_.boardHandle_);
  if (status)
    {
      Log("[HwManager]::[ERROR]::VME Crate RESET ERROR",1);
      throw config_exception();
    }

  usleep(10000); 

  return 0;

}

// --- Clear
void HwManager::Clear(){
	// --- reset to un-initialized/ un-config state	
	//runControl_=false;
	for(int i=0;i<hw_.size();i++)
		hw_[i]->Clear();
	return;
}

void HwManager::Print(){
	Log("[2] Printing configuratio",2);
	for(vector<Board*>::iterator iBoard=hw_.begin();iBoard!=hw_.end();iBoard++)
		{
		int r = (*iBoard)->Print();// 0-1 are ok status
		if(r)Log( string("[2] Error on Board")+(*iBoard)->GetType(),2);
		}
	return ; 
}


void HwManager::Read(int i,vector<WORD> &v)
{
	v.clear();
	hw_[i]->Read(v);
	return;
}

void HwManager::ReadAll(dataType&R){ // don't construct the all event
	vector<WORD> v; 
	for(int i=0;i< hw_.size();i++)
	{
		hw_[i]->Read(v);
		BoardId bId;
			bId.crateId_  = crateId_;
			bId.boardType_= GetBoardTypeId( hw_[i]->GetType() ); // WORD
			bId.boardId_  = hw_[i]->GetId() ; 
		EventBuilder::BoardToStream( R, bId , v )  ;
	}
	return ;
}

void  HwManager::BufferClearAll(){
	for(int i=0;i< hw_.size();i++)
		hw_[i]->BufferClear();
	return;
}


void HwManager::ClearBusy(){
	if (trigBoard_.boardIndex_<0 ) 
	  {
	    ostringstream s;
	    s << "[HwManager]::[ERROR]::Trigger Board not available";
	    Log(s.str(),1);
	    throw hw_exception();
	  }
	
	int status = dynamic_cast<TriggerBoard*>(hw_[trigBoard_.boardIndex_])->ClearBusy();
	if ( status )
	  {
	    ostringstream s;
	    s << "[HwManager]::[ERROR]::ClearBusy failed";
	    Log(s.str(),1);
	    throw hw_exception();
	  }
}

bool HwManager::TriggerReceived(){
	if (trigBoard_.boardIndex_<0 ) 
	  {
	    ostringstream s;
	    s << "[HwManager]::[ERROR]::Trigger Board not available";
	    Log(s.str(),1);
	    throw hw_exception();
	  }

	return dynamic_cast<TriggerBoard*>(hw_[trigBoard_.boardIndex_])->TriggerReceived();
}

void HwManager::TriggerAck(){
	if (trigBoard_.boardIndex_<0 ) 
	  {	    
	    ostringstream s;
	    s << "[HwManager]::[ERROR]::Trigger Board not available";
	    Log(s.str(),1);
	    throw hw_exception();
	  }

	int status=dynamic_cast<TriggerBoard*>(hw_[trigBoard_.boardIndex_])->TriggerAck();
	if ( status )
	  {
	    ostringstream s;
	    s << "[HwManager]::[ERROR]::TriggerAck failed " << status;
	    Log(s.str(),1);
	    throw hw_exception();
	  }
}

bool HwManager::SignalReceived(CMD_t signal)
{
	if (ioControlBoard_.boardIndex_<0 ) 
	  {	    
	    ostringstream s;
	    s << "[HwManager]::[ERROR]::IOControl Board not available";
	    Log(s.str(),1);
	    throw hw_exception();
	  }
	return dynamic_cast<IOControlBoard*>(hw_[ioControlBoard_.boardIndex_])->SignalReceived(signal);
}


void HwManager::SetTriggerStatus(TRG_t triggerType,TRG_STATUS_t triggerStatus)
{
	if (ioControlBoard_.boardIndex_<0 ) 
	  {	    
	    ostringstream s;
	    s << "[HwManager]::[ERROR]::IOControl Board not available";
	    Log(s.str(),1);
	    throw hw_exception();
	  }
	int status=dynamic_cast<IOControlBoard*>(hw_[ioControlBoard_.boardIndex_])->SetTriggerStatus(triggerType,triggerStatus);
	if ( status )
	  {
	    ostringstream s;
	    s << "[HwManager]::[ERROR]::SetTriggerStatus failed " << status;
	    Log(s.str(),1);
	    throw hw_exception();
	  }
}

// ------------------ STATIC ----------------
BoardTypes_t HwManager::GetBoardTypeId(string type){
	if (type=="TIME" ) return _TIME_;
	else if( type=="CAEN_VX718") return _CAENVX718_;
	else if( type=="CAEN_V1742") return _CAENV1742_;
	else if( type=="CAEN_V513") return _CAENV513_;
	else if( type=="CAEN_V262") return _CAENV262_;
	else if( type=="CAEN_V792") return _CAENV792_;
	else if( type=="CAEN_V1290") return _CAENV1290_;
	else if( type=="CAEN_V1495PU") return _CAENV1495PU_;
	else if( type=="CAEN_V560") return _CAENV560_;
	else return _UNKWN_;
}
