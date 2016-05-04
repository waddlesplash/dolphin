// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "../Core.h"
#include "../ConfigManager.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/EXI.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceAMBaseboard.h"

CEXIAMBaseboard::CEXIAMBaseboard()
	: m_position(0)
	, m_have_irq(false)
{
	std::string backup_Filename(File::GetUserPath(D_TRIUSER_IDX) + "tribackup_" + SConfig::GetInstance().GetUniqueID() + ".bin");

	if( File::Exists( backup_Filename ) )
	{
		m_backup = new File::IOFile( backup_Filename, "rb+" );
	}
	else
	{
		m_backup = new File::IOFile( backup_Filename, "wb+" );
	}
}

void CEXIAMBaseboard::SetCS(int cs)
{
	DEBUG_LOG(SP1, "AM-BB ChipSelect=%d", cs);
	if (cs)
		m_position = 0;
}

bool CEXIAMBaseboard::IsPresent() const
{
	return true;
}

void CEXIAMBaseboard::TransferByte(u8& _byte)
{
	/*
	ID:
		00 00 xx xx xx xx
		xx xx 06 04 10 00
	CMD:
		01 00 00 b3 xx
		xx xx xx xx 04
	exi_lanctl_write:
		ff 02 01 63 xx
		xx xx xx xx 04
	exi_imr_read:
		86 00 00 f5 xx xx xx
		xx xx xx xx 04 rr rr
	exi_imr_write:
		87 80 5c 17 xx
		xx xx xx xx 04

	exi_isr_read:
		82 .. .. .. xx xx xx
		xx xx xx xx 04 rr rr
		3 byte command, 1 byte checksum
	*/
	DEBUG_LOG(SP1, "AM-BB > %02x", _byte);
	if (m_position < 4)
	{
		m_command[m_position] = _byte;
		_byte = 0xFF;
	}

	if ((m_position >= 2) && (m_command[0] == 0 && m_command[1] == 0))
	{
		// Read serial ID
		_byte = "\x06\x04\x10\x00"[(m_position - 2) & 3];
	}
	else if (m_position == 3)
	{
		unsigned int checksum = (m_command[0] << 24) | (m_command[1] << 16) | (m_command[2] << 8);
		unsigned int bit = 0x80000000UL;
		unsigned int check = 0x8D800000UL;
		while (bit >= 0x100)
		{
			if (checksum & bit)
				checksum ^= check;
			check >>= 1;
			bit >>= 1;
		}

		if (m_command[3] != (checksum & 0xFF))
			DEBUG_LOG(SP1, "AM-BB cs: %02x, w: %02x", m_command[3], checksum & 0xFF);
	}
	else
	{
		if (m_position == 4)
		{
			switch (m_command[0])
			{
			case 0x01:
				m_backoffset = (m_command[1] << 8) | m_command[2];
				DEBUG_LOG(SP1,"AM-BB COMMAND: Backup Offset:%04X", m_backoffset );
				m_backup->Seek( m_backoffset, SEEK_SET );
				_byte = 0x01;
				break;
			case 0x02:
				DEBUG_LOG(SP1,"AM-BB COMMAND: Backup Write:%04X-%02X", m_backoffset, m_command[1] );
				m_backup->WriteBytes( &m_command[1], 1 );
				m_backup->Flush();
				_byte = 0x01;
				break;
			case 0x03:
				DEBUG_LOG(SP1,"AM-BB COMMAND: Backup Read :%04X", m_backoffset );
				_byte = 0x01;
				break;
			// Unknown
			case 0x05:
				_byte = 0x04;
				break;
			// Clear IRQ
			case 0x82:
				WARN_LOG(SP1,"AM-BB COMMAND: 0x82 :%02X %02X", m_command[1], m_command[2] );
				_byte = 0x04;
				break;
			// Unknown
			case 0x83:
				WARN_LOG(SP1,"AM-BB COMMAND: 0x83 :%02X %02X", m_command[1], m_command[2] );
				_byte = 0x04;
				break;
			// Unknown - 2 byte out
			case 0x86:
				WARN_LOG(SP1,"AM-BB COMMAND: 0x86 :%02X %02X", m_command[1], m_command[2] );
				_byte = 0x04;
				break;
			// Unknown
			case 0x87:
				WARN_LOG(SP1,"AM-BB COMMAND: 0x87 :%02X %02X", m_command[1], m_command[2] );
				_byte = 0x04;
				break;
			// Unknown
			case 0xFF:
				WARN_LOG(SP1,"AM-BB COMMAND: 0xFF :%02X %02X", m_command[1], m_command[2] );
				if( (m_command[1] == 0) && (m_command[2] == 0) )
				{
					m_have_irq = true;
					m_irq_timer = 0;
					m_irq_status = 0x02;
				}
				if( (m_command[1] == 2) && (m_command[2] == 1) )
				{
					m_irq_status = 0;
				}
				_byte = 0x04;
				ExpansionInterface::UpdateInterrupts();
				break;
			default:
				_byte = 4;
				ERROR_LOG(SP1, "AM-BB COMMAND: %02x %02x %02x", m_command[0], m_command[1], m_command[2]);
				break;
			}
		}
		else if (m_position > 4)
		{
			switch (m_command[0])
			{
			// Read backup - 1 byte out
			case 0x03:
				m_backup->ReadBytes( &_byte, 1);
				break;
			// IMR - 2 byte out
			case 0x82:
				if(m_position == 6)
				{
					_byte = m_irq_status;
					m_have_irq = false;
				}
				else
				{
					_byte = 0x00;
				}
				break;
			// ? - 2 byte out
			case 0x86:
				_byte = 0x00;
				break;
			default:
				_dbg_assert_msg_(SP1, 0, "Unknown AM-BB command");
				break;
			}
		}
		else
		{
			_byte = 0xFF;
		}
	}
	DEBUG_LOG(SP1, "AM-BB < %02x", _byte);
	m_position++;
}

bool CEXIAMBaseboard::IsInterruptSet()
{
	if (m_have_irq)
	{
		DEBUG_LOG(SP1, "AM-BB IRQ");
		if( ++m_irq_timer > 4 )
			m_have_irq = false;
		return 1;
	}
	else
	{
		return 0;
	}
}

void CEXIAMBaseboard::DoState(PointerWrap &p)
{
	p.Do(m_position);
	p.Do(m_have_irq);
	p.Do(m_command);
}
CEXIAMBaseboard::~CEXIAMBaseboard()
{
	m_backup->Close();
	delete m_backup;
}

