// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/EXI_Device.h"

class PointerWrap;

class CEXIAMBaseboard : public IEXIDevice
{
public:
	CEXIAMBaseboard();

	void SetCS(int _iCS) override;
	bool IsPresent() const override;
	bool IsInterruptSet() override;
	void DoState(PointerWrap &p) override;

	~CEXIAMBaseboard();

private:
	void TransferByte(u8& _uByte) override;
	int m_position;
	bool m_have_irq;
	u32 m_irq_timer;
	u32 m_irq_status;
	unsigned char m_command[4];
	unsigned short m_backoffset;
	File::IOFile *m_backup;
};
