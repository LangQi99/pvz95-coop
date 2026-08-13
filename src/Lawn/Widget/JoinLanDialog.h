/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "LawnDialog.h"
#include "../../Multiplayer/UdpSocket.h"
#include "widget/EditListener.h"

#include <optional>
#include <string>

namespace Sexy
{
	class EditWidget;
}

enum class JoinLanField
{
	NONE,
	ADDRESS,
	PORT
};

class JoinLanDialog : public LawnDialog, public Sexy::EditListener
{
public:
	LawnApp* mApp;
	Sexy::EditWidget* mAddressEditWidget;
	Sexy::EditWidget* mPortEditWidget;
	std::string mValidationError;

public:
	explicit JoinLanDialog(LawnApp* theApp);
	~JoinLanDialog() override;

	void Resize(int theX, int theY, int theWidth, int theHeight) override;
	void AddedToManager(WidgetManager* theWidgetManager) override;
	void RemovedFromManager(WidgetManager* theWidgetManager) override;
	void Draw(Graphics* g) override;
	void EditWidgetText(int theId, const std::string& theString) override;

	std::optional<PvzMultiplayer::Ipv4Endpoint> GetEndpoint(
		std::string& theError, JoinLanField& theInvalidField) const;
	void ShowValidationError(std::string theError, JoinLanField theInvalidField);
};
