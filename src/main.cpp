#include <iostream>

#include <pv/epicsException.h>
#include <pv/createRequest.h>
#include <pv/event.h>
#include <pv/pvData.h>
#include <pv/clientFactory.h>
#include <pv/pvAccess.h>

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

class MyRequester : public virtual Requester
{
	public:
		MyRequester(string const & requester_name)
			: requester_name(requester_name)
			{}

		string getRequesterName() 
			{ return requester_name; }
		
		void message(string const & message, MessageType messageType);
	private:
		string requester_name;
};

void MyRequester::message(string const & message, MessageType messageType)
{
	    cout << getMessageTypeName(messageType) << ": "
		     << requester_name << " "
			 << message << endl;
}

class MyChannelRequester : public virtual MyRequester, public virtual ChannelRequester
{
	public:
		MyChannelRequester() : MyRequester("MyChannelRequester")
			{}
		
		void channelCreated(const Status & status, Channel::shared_pointer const & channel);
		void channelStateChange(Channel::shared_pointer const & channel, 
			Channel::ConnectionState connectionState);

		boolean waitUntilConnected(double timeOut)
		{
			boolean result = connect_event.wait(timeOut);
			
			if(!result)
				cerr << "Channel '" << channel_name << "' failed to connect\n";

			return result;
		}

	private:
		string channel_name;
		Event connect_event;
};

/* Upon successfuly channel creation, print message */
void MyChannelRequester::channelCreated(const Status & status, Channel::shared_pointer const & channel)
{
	channel_name = channel->getChannelName();
	cout << channel_name << " created, " << status << endl;
}

/* Upon channel state change, print the channel's current state */
void MyChannelRequester::channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState)
{
	cout << channel->getChannelName() << " state: "
		 << Channel::ConnectionStateNames[connectionState]
		 << " (" << connectionState << ") " << endl;
	if (connectionState == Channel::CONNECTED)
		connect_event.signal();
}

class MyChannelGetRequester : public virtual MyRequester, public virtual ChannelGetRequester
{
	public:
		MyChannelGetRequester() : MyRequester("MyChannelGetRequester")
			{}

		void channelGetConnect(const Status & status,
				ChannelGet::shared_pointer const & channelGet,
				Structure::const_shared_pointer const & structure);
		
		void getDone(const Status & status,
				ChannelGet::shared_pointer const & channelGet,
				PVStructure::shared_pointer const & pvStructure,
				BitSet::shared_pointer const & bitSet);

		boolean waitUntilDone(double timeOut)
		{
			boolean result = done_event.wait(timeOut);
			if (!result)
				cout << "Get request failed\n";
			return result;
		}

	private:
		Event done_event;
};

void MyChannelGetRequester::channelGetConnect(const Status & status,
				ChannelGet::shared_pointer const & channelGet,
				Structure::const_shared_pointer const & structure)
{
/* Upon success print a message, dump the request structure (sans values) to sdout, and call get() */
	if(status.isSuccess()) {
		cout << "ChannelGet for " << channelGet->getChannel()->getChannelName()
			 << " connected, " << status << endl;
		structure->dump(cout);

		channelGet->get();

/* Otherwise print an error message and signal that we are done. */
	} else {
		cout << "ChannelGet for " << channelGet->getChannel()->getChannelName()
			 << " problem, " << status << endl;
		done_event.signal();
	}
}

void MyChannelGetRequester::getDone(const Status & status,
			ChannelGet::shared_pointer const & channelGet,
			PVStructure::shared_pointer const & pvStructure,
			BitSet::shared_pointer const & bitSet)
{
	cout << "ChannelGet for " << channelGet->getChannel()->getChannelName()
		 << " finished, " << status << endl;

/* Upon success dump the entire request structure to stdout and signal that we are done. */
	if (status.isSuccess()) {
		pvStructure->dumpValue(cout);
		done_event.signal();
	}
}

void getValue(string const & channel_name, string const & request, double timeout)
{
	static string prev_channel("");
	static shared_ptr<Channel> channel;

/* Request the "pva" channel provider. pva - > pvAccess. as opposed to ca -> channel access */
	ChannelProvider::shared_pointer channelProvider = 
		getChannelProviderRegistry()->getProvider("pva");
	
	if (!channelProvider)
		THROW_EXCEPTION2(runtime_error, "No channel provider");

/* Create a channel requester. This will allow us to create a channel to the pvRecord */
	shared_ptr<MyChannelRequester> channelRequester(new MyChannelRequester());

/* Create a channel using the channel requester. Channels are expensive, so only create a new one
 * if a new channel name is given.
 * */
	if (prev_channel.compare("") == 0 || prev_channel.compare(channel_name) != 0) {
		
		channel = channelProvider->createChannel(channel_name, channelRequester);
		
		channelRequester->waitUntilConnected(timeout);
		
		prev_channel = channel_name;
	}
	

/* Create a request structure. This will be filled with the data retrieved from the record. */
	shared_ptr<PVStructure> pvRequest = CreateRequest::create()->createRequest(request);
	
/* Create a channelGetRequester. This handles connecting the get, and then notifying upon completion. */
	shared_ptr<MyChannelGetRequester> channelGetRequester(new MyChannelGetRequester());

/* Create a channelGet. This will actually perform the get operation. */
	shared_ptr<ChannelGet> channelGet = channel->createChannelGet(channelGetRequester, pvRequest);
	
	channelGetRequester->waitUntilDone(timeout);
}

int main (int argc, char ** argv)
{
	string channel_name("PVRubyte");
	string request("field(value)");
	double timeout = 2.0;

	try {
	/* Starts the client side pva provider */
		ClientFactory::start();
	
	/* Issue a get request for the specified channel */
		getValue(channel_name, request, timeout);
	
	/* This will reuse the same channel. */
		getValue(channel_name, request, timeout);
	
		channel_name.assign("PVRulong");

	/* This will create a new channel. */
		getValue(channel_name, request, timeout);
		
		ClientFactory::stop();

	} catch (exception &ex) {
		cerr << ex.what() << endl;
		return -1;
	}

	return 0;
}
