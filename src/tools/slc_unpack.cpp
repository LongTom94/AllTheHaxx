#include <stdio.h>

#include <base/system.h>
#include <engine/storage.h>
#include <engine/serverbrowser.h>
#include <engine/client/serverbrowser.h>


// vars
IStorageTW *s_pStorage = 0;

// arguments
static const char *as_pIn = "tmp/cache/serverlist";
static const char *as_pOut = 0;
namespace TYPE
{
	enum
	{
		INVALID=-1,
		CSV,
		FAV,
		ADDR
	};
};
static int as_Type = TYPE::INVALID;



static void Print(const char *fmt, ...)
{
	va_list args;
	char msg[512];

	va_start(args, fmt);
#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(msg, sizeof(msg), fmt, args);
#else
	vsnprintf(msg, sizeof(msg), fmt, args);
#endif
	va_end(args);
	printf("%s\n", msg);
}


int PrintUsage(const char *argv0)
{
	char aDefaultCachePath[512] = {0};
	s_pStorage->GetCompletePath(IStorageTW::TYPE_SAVE, "tmp/cache/serverlist", aDefaultCachePath, sizeof(aDefaultCachePath));

	Print("----------------------------------------------");
	Print("-    AllTheHaxx serverlist cache unpacker    -");
	Print("-        (c) 2016 The AllTheHaxx Team        -");
	Print("-             Author: Henritees              -");
	Print("----------------------------------------------\n");
	Print("This tool will unpack the contents of the serverlist cache file into");
	Print("a normal text file that can be read by humans.");
	Print("Features are:");
	Print("- configurable csv outputting");
	Print("- plain address outputting");
	Print("- generation of a teeworlds config file to add all servers as favorites");
	Print("");
	Print("Usage: %s -t/--type <'csv', 'fav', 'addr'> [-o/--out <path>]", argv0);
	Print("Options are: (case sensitive, shorts can't be combined)");
	Print("    -h / --help // Prints out this help and exits");
	Print("    -o / --out <path> // where to output everything (if omitted: stdout");
	Print("    -i / --in <path=\"%s\">", aDefaultCachePath);
	Print("    -t / --type <TYPE>", aDefaultCachePath);
	Print("      TYPE: fav // generates a config file which can add all servers to your favorites");
	Print("      TYPE: addr // outputs the plain addresses, no server infos");
	Print("      TYPE: csv // outputs a csv file");

	return 0;
}

int ParseArgs(int argc, const char **argv)
{
	for(int i = 1; i < argc; i++)
	{
		const char *pArg = argv[i];

		// ignore invalid args
		if(str_length(pArg) < 2)
			return 0;

		// flag args
		if(*(pArg++) != '-')
			continue;

		#define ARG(Short, Long) ((*pArg == (Short)) || (*pArg == '-' && str_comp_nocase(pArg+1, (Long)) == 0))
		#define CHECK(Num) if(i+(Num) >= argc) return 1

		if(ARG('h', "help"))
			PrintUsage(argv[0]);
		else if(ARG('i', "in"))
		{
			CHECK(1);
			as_pIn = argv[i + 1];
		}
		else if(ARG('o', "out"))
		{
			CHECK(1);
			as_pOut = argv[i + 1];
		}
		else if(ARG('t', "type"))
		{
			CHECK(1);
			const char *pNext = argv[i+1];
			if(str_comp_nocase(pNext, "fav") == 0)
				as_Type = TYPE::FAV;
			else if(str_comp_nocase(pNext, "addr") == 0)
				as_Type = TYPE::ADDR;
			else if(str_comp_nocase(pNext, "csv") == 0)
				as_Type = TYPE::CSV;
			else
				return PrintUsage(argv[0]);
		}

		#undef ARG
		#undef CHECK

	}

	return 0;
}


int main(int argc, const char **argv)
{
	/* dbg_logger_stdout(); */
	s_pStorage = CreateStorage("Teeworlds", IStorageTW::STORAGETYPE_CLIENT, argc, argv);
	Print("");


	// manage arguments
	if(argc == 1)
		return PrintUsage(argv[0]);

	if(ParseArgs(argc, argv) != 0)
		return PrintUsage(argv[0]);


	// open file
	IOHANDLE File = s_pStorage->OpenFile(as_pIn, IOFLAG_READ, IStorageTW::TYPE_ALL);
	if(!File)
	{
		Print("opening cache file '%s' failed.", as_pIn);
		return 1;// false;
	}

	// get version
	{
		char v; io_read(File, &v, 1);
		if(as_pOut)
			Print("loading serverlist from cache...");
		if(v != IServerBrowser::CACHE_VERSION)
			if(as_pOut)
				Print("file version doesn't match, we may fail! (%i != %i)", v, IServerBrowser::CACHE_VERSION);
	}

	// get number of servers
	int NumServers = 0;
	io_read(File, &NumServers, sizeof(NumServers));
	Print("serverlist cache entries: %i", NumServers);

	// get length of array - only needed to get the format of the file correctly
	{int NumServerCapacity = 0;
	io_read(File, &NumServerCapacity, sizeof(NumServerCapacity));}

	// read the data from the file
	CServerInfo *pServerInfos = (CServerInfo*)mem_alloc(sizeof(CServerInfo)*NumServers, 0);
	io_read(File, pServerInfos, sizeof(CServerInfo)*NumServers);
	io_close(File);

	if(as_pOut)
	{
		File = io_open(as_pOut, IOFLAG_WRITE);
		if(!File)
		{
			Print("Failed to open file '%s' for writing", as_pOut);
			return -1;
		}
	}
	else
		File = io_stdout();

	if(as_Type == TYPE::ADDR)
	{
		for(int i = 0; i < NumServers; i++)
		{
			io_write(File, pServerInfos[i].m_aAddress, (unsigned int)str_length(pServerInfos[i].m_aAddress));
			io_write_newline(File);
		}
	}
	else if(as_Type == TYPE::FAV)
	{
		char aBuf[128];
		for(int i = 0; i < NumServers; i++)
		{
			str_format(aBuf, sizeof(aBuf), "add_favorite %s", pServerInfos[i].m_aAddress);
			io_write(File, aBuf, (unsigned int)str_length(aBuf));
			io_write_newline(File);
		}
	}
	else if(as_Type == TYPE::CSV)
	{
		// header
		char aBuf[4096];
		str_format(aBuf, sizeof(aBuf), "address\tname\ttype\tmap\tplayers\tping\tscoreboard");
		io_write(File, aBuf, (unsigned int)str_length(aBuf));
		io_write_newline(File);

		for(int i = 0; i < NumServers; i++)
		{
			const CServerInfo *e = &pServerInfos[i];
			char aScoreBoard[2048] = {0};
			for(int n = 0; n < e->m_MaxClients; n++)
			{
				const CServerInfo::CClient *c = &(e->m_aClients[i]);
				if(str_length(c->m_aName) <= 0)
					continue;

				char aScore[8];
				if(c->m_Player)
					str_format(aScore, sizeof(aScore), "%i", c->m_Score);
				else
					str_format(aScore, sizeof(aScore), "%s", "SPEC");
				char aEntry[128];
				str_format(aEntry, sizeof(aEntry), "{[%s]  '%s'  '%s'} ", aScore, c->m_aName, c->m_aClan);
				str_append(aScoreBoard, aEntry, sizeof(aScoreBoard));
			}

			str_format(aBuf, sizeof(aBuf), "%s\t%s\t%s\t%s\t%i/%i[%i]\t%i\t%s",
					   e->m_aAddress,
					   e->m_aName,
					   e->m_aGameType,
					   e->m_aMap,
					   e->m_NumClients, e->m_MaxClients, e->m_NumClients-e->m_NumPlayers,
					   e->m_Latency,
					   aScoreBoard);
			io_write(File, aBuf, (unsigned int)str_length(aBuf));
			io_write_newline(File);
		}
	}
	else
		return PrintUsage(argv[0]);

	io_flush(File);
	io_close(File);
	Print("Export done.");

	mem_free(pServerInfos);
	return 0;
}