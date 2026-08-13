/*
 * Copyright (C) 2026 LangQi99
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "JoinLanDialog.h"

#include "../../LawnApp.h"
#include "../../Multiplayer/Protocol.h"
#include "../../Resources.h"
#include "../LawnCommon.h"
#include "GameButton.h"
#include "graphics/Font.h"
#include "widget/EditWidget.h"
#include "widget/WidgetManager.h"

#include <cstdint>

namespace
{
	constexpr int ADDRESS_EDIT_ID = 0;
	constexpr int PORT_EDIT_ID = 1;
}

JoinLanDialog::JoinLanDialog(LawnApp* theApp) : LawnDialog(
	theApp,
	Dialogs::DIALOG_JOIN_LAN,
	true,
	"JOIN LAN ROOM",
	"",
	"",
	Dialog::BUTTONS_OK_CANCEL)
{
	mApp = theApp;
	mVerticalCenterText = false;
	mAddressEditWidget = CreateEditWidget(ADDRESS_EDIT_ID, this, this);
	mAddressEditWidget->mMaxChars = 15;
	mAddressEditWidget->AddWidthCheckFont(Sexy::FONT_BRIANNETOD16, 260);
	mAddressEditWidget->SetText("127.0.0.1", true);

	mPortEditWidget = CreateEditWidget(PORT_EDIT_ID, this, this);
	mPortEditWidget->mMaxChars = 5;
	mPortEditWidget->AddWidthCheckFont(Sexy::FONT_BRIANNETOD16, 260);
	mPortEditWidget->SetText(std::to_string(PvzMultiplayer::DEFAULT_GAME_PORT), true);

	mLawnYesButton->SetLabel("Join");
	mLawnNoButton->SetLabel("Cancel");
	CalcSize(190, 150);
}

JoinLanDialog::~JoinLanDialog()
{
	delete mAddressEditWidget;
	delete mPortEditWidget;
}

void JoinLanDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);
	int anEditX = mContentInsets.mLeft + 20;
	int anEditWidth = mWidth - mContentInsets.mLeft - mContentInsets.mRight - 40;
	int aFirstEditY = mHeight - 218;
	mAddressEditWidget->Resize(anEditX, aFirstEditY, anEditWidth, 28);
	mPortEditWidget->Resize(anEditX, aFirstEditY + 64, anEditWidth, 28);
}

void JoinLanDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	AddWidget(mAddressEditWidget);
	AddWidget(mPortEditWidget);
	theWidgetManager->SetFocus(mAddressEditWidget);
}

void JoinLanDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	RemoveWidget(mAddressEditWidget);
	RemoveWidget(mPortEditWidget);
}

void JoinLanDialog::Draw(Graphics* g)
{
	LawnDialog::Draw(g);
	g->SetFont(Sexy::FONT_BRIANNETOD16);
	g->SetColor(Color(48, 30, 16));
	g->DrawString("Enter the host's numeric IPv4 address and game port.",
		mAddressEditWidget->mX, mAddressEditWidget->mY - 30);
	g->DrawString("IP address", mAddressEditWidget->mX, mAddressEditWidget->mY - 7);
	g->DrawString("Port", mPortEditWidget->mX, mPortEditWidget->mY - 7);
	DrawEditBox(g, mAddressEditWidget);
	DrawEditBox(g, mPortEditWidget);

	if (!mValidationError.empty())
	{
		g->SetColor(Color(160, 20, 20));
		g->WriteWordWrapped(
			Rect(mPortEditWidget->mX, mPortEditWidget->mY + 36, mPortEditWidget->mWidth, 45),
			mValidationError,
			Sexy::FONT_BRIANNETOD16->GetLineSpacing(),
			-1);
	}
}

void JoinLanDialog::EditWidgetText(int theId, const std::string& theString)
{
	(void)theString;
	if (theId == ADDRESS_EDIT_ID && mWidgetManager)
	{
		mWidgetManager->SetFocus(mPortEditWidget);
		return;
	}
	mApp->ButtonDepress(mId + 2000);
}

std::optional<PvzMultiplayer::Ipv4Endpoint> JoinLanDialog::GetEndpoint(
	std::string& theError, JoinLanField& theInvalidField) const
{
	theError.clear();
	theInvalidField = JoinLanField::NONE;

	const std::string& aPortText = mPortEditWidget->mString;
	if (aPortText.empty())
	{
		theError = "Enter a port number from 1 to 65535.";
		theInvalidField = JoinLanField::PORT;
		return std::nullopt;
	}

	uint32_t aPort = 0;
	for (char aChar : aPortText)
	{
		if (aChar < '0' || aChar > '9')
		{
			theError = "Port must contain digits only and be from 1 to 65535.";
			theInvalidField = JoinLanField::PORT;
			return std::nullopt;
		}
		aPort = aPort * 10U + static_cast<uint32_t>(aChar - '0');
		if (aPort > 65535U)
		{
			theError = "Port must be from 1 to 65535.";
			theInvalidField = JoinLanField::PORT;
			return std::nullopt;
		}
	}
	if (aPort == 0)
	{
		theError = "Port must be from 1 to 65535.";
		theInvalidField = JoinLanField::PORT;
		return std::nullopt;
	}

	auto anEndpoint = PvzMultiplayer::Ipv4Endpoint::Parse(
		mAddressEditWidget->mString, static_cast<uint16_t>(aPort));
	if (!anEndpoint)
	{
		theError = "Enter a numeric IPv4 address such as 127.0.0.1.";
		theInvalidField = JoinLanField::ADDRESS;
		return std::nullopt;
	}

	return anEndpoint;
}

void JoinLanDialog::ShowValidationError(std::string theError, JoinLanField theInvalidField)
{
	mValidationError = std::move(theError);
	MarkDirty();
	if (!mWidgetManager)
		return;
	if (theInvalidField == JoinLanField::PORT)
		mWidgetManager->SetFocus(mPortEditWidget);
	else
		mWidgetManager->SetFocus(mAddressEditWidget);
}
