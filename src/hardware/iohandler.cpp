#include "vDos.h"
#include "inout.h"

IO_WriteHandler * io_writehandlers[2][IO_MAX];
IO_ReadHandler * io_readhandlers[2][IO_MAX];

static Bitu IO_ReadBlocked(Bitu /*port*/, Bitu /*iolen*/)
	{
	return ~0;
	}

static void IO_WriteBlocked(Bitu /*port*/,Bitu /*val*/,Bitu /*iolen*/)
	{
	}

static Bitu IO_ReadDefault(Bitu port, Bitu iolen)
	{
	switch (iolen)
		{
	case 1:
		io_readhandlers[0][port] = IO_ReadBlocked;
		return 0xff;
	case 2:
		return (io_readhandlers[0][port](port, 1)) | (io_readhandlers[0][port+1](port+1, 1) << 8);
		}
	return 0;
	}

void IO_WriteDefault(Bitu port, Bitu val, Bitu iolen)
	{
	switch (iolen)
		{
	case 1:
		io_writehandlers[0][port] = IO_WriteBlocked;
		break;
	case 2:
		io_writehandlers[0][port+0](port+0, val & 0xff, 1);
		io_writehandlers[0][port+1](port+1, (val >> 8) & 0xff, 1);
		break;
		}
	}

void IO_RegisterReadHandler(Bitu port, IO_ReadHandler * handler, Bitu mask, Bitu range)
	{
	while (range--)
		{
		if (mask&IO_MB)
			io_readhandlers[0][port] = handler;
		if (mask&IO_MW)
			io_readhandlers[1][port] = handler;
		port++;
		}
	}

void IO_RegisterWriteHandler(Bitu port, IO_WriteHandler * handler, Bitu mask, Bitu range)
	{
	while (range--)
		{
		if (mask&IO_MB)
			io_writehandlers[0][port] = handler;
		if (mask&IO_MW)
			io_writehandlers[1][port] = handler;
		port++;
		}
	}

void IO_FreeReadHandler(Bitu port, Bitu mask, Bitu range)
	{
	while (range--)
		{
		if (mask&IO_MB)
			io_readhandlers[0][port] = IO_ReadDefault;
		if (mask&IO_MW)
			io_readhandlers[1][port] = IO_ReadDefault;
		port++;
		}
	}

void IO_FreeWriteHandler(Bitu port, Bitu mask, Bitu range)
	{
	while (range--)
		{
		if (mask&IO_MB)
			io_writehandlers[0][port] = IO_WriteDefault;
		if (mask&IO_MW)
			io_writehandlers[1][port] = IO_WriteDefault;
		port++;
		}
	}

void IO_ReadHandleObject::Install(Bitu port, IO_ReadHandler * handler, Bitu mask, Bitu range)
	{
	if (!installed)
		{
		installed = true;
		m_port = port;
		m_mask = mask;
		m_range = range;
		IO_RegisterReadHandler(port, handler, mask, range);
		}
	else
		E_Exit("IO_readHandler allready installed port %x", port);
	}

IO_ReadHandleObject::~IO_ReadHandleObject()
	{
	if (installed)
		IO_FreeReadHandler(m_port, m_mask, m_range);
	}

void IO_WriteHandleObject::Install(Bitu port, IO_WriteHandler * handler, Bitu mask, Bitu range)
	{
	if (!installed)
		{
		installed = true;
		m_port = port;
		m_mask = mask;
		m_range = range;
		IO_RegisterWriteHandler(port, handler, mask, range);
		}
	else
		E_Exit("IO_writeHandler allready installed port %x", port);
	}

IO_WriteHandleObject::~IO_WriteHandleObject()
	{
	if (installed)
		IO_FreeWriteHandler(m_port, m_mask, m_range);
	}

void IO_WriteB(Bitu port, Bitu val)
	{
	io_writehandlers[0][port](port, val, 1);
	}

void IO_WriteW(Bitu port,Bitu val)
	{
	io_writehandlers[1][port](port, val, 2);
	}

void IO_WriteD(Bitu port, Bitu val)
	{
	io_writehandlers[1][port+0](port+0, val & 0xffff, 2);
	io_writehandlers[1][port+2](port+2, (val >> 16) & 0xffff, 2);
	}

Bitu IO_ReadB(Bitu port)
	{
	return io_readhandlers[0][port](port, 1);
	}

Bitu IO_ReadW(Bitu port)
	{
	return io_readhandlers[1][port](port, 2);
	}

Bitu IO_ReadD(Bitu port)
	{
	return (io_readhandlers[1][port](port, 2)) | (io_readhandlers[1][port+2](port+2, 2) << 16);
	}

void IO_Init()
	{
	IO_FreeReadHandler(0, IO_MA, IO_MAX);
	IO_FreeWriteHandler(0, IO_MA, IO_MAX);
	}
