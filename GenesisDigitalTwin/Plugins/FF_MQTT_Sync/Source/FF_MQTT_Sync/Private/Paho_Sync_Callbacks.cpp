#include "Paho_Sync_Manager.h"

#pragma region Internals

bool APaho_Manager_Sync::SetSSLParams(FString In_Protocol, FPahoSslOptions In_Options)
{
	if (In_Protocol == "wss" || In_Protocol == "mqtts" || In_Protocol == "ssl" || In_Protocol == "WSS" || In_Protocol == "MQTTS" || In_Protocol == "SSL")
	{
		this->SSL_Options = MQTTClient_SSLOptions_initializer;
		this->SSL_Options.enableServerCertAuth = 0;
		this->SSL_Options.verify = 1;

		if (!In_Options.CAPath.IsEmpty() && FPaths::FileExists(In_Options.CAPath))
		{
			this->SSL_Options.CApath = (const char*)StringCast<UTF8CHAR>(*In_Options.CAPath).Get();
		}

		if (!In_Options.Path_KeyStore.IsEmpty() && FPaths::FileExists(In_Options.Path_KeyStore))
		{
			this->SSL_Options.keyStore = (const char*)StringCast<UTF8CHAR>(*In_Options.Path_KeyStore).Get();
		}

		if (!In_Options.Path_TrustStore.IsEmpty() && FPaths::FileExists(In_Options.Path_TrustStore))
		{
			this->SSL_Options.trustStore = (const char*)StringCast<UTF8CHAR>(*In_Options.Path_TrustStore).Get();
		}

		if (!In_Options.Path_PrivateKey.IsEmpty() && FPaths::FileExists(In_Options.Path_PrivateKey))
		{
			this->SSL_Options.privateKey = (const char*)StringCast<UTF8CHAR>(*In_Options.Path_PrivateKey).Get();
		}

		if (!In_Options.PrivateKeyPass.IsEmpty())
		{
			this->SSL_Options.privateKeyPassword = (const char*)StringCast<UTF8CHAR>(*In_Options.PrivateKeyPass).Get();
		}

		if (!In_Options.CipherSuites.IsEmpty())
		{
			this->SSL_Options.enabledCipherSuites = (const char*)StringCast<UTF8CHAR>(*In_Options.CipherSuites).Get();
		}

		return true;
	}

	else
	{
		return false;
	}
}

#pragma endregion Internals

#pragma region Callbacks

void APaho_Manager_Sync::MessageDelivered(void* CallbackContext, MQTTClient_deliveryToken In_DeliveryToken)
{
	// Delivery notifications are not used by the digital twin flow. Avoid
	// broadcasting a non-thread-safe Blueprint delegate from Paho callback
	// threads during PIE teardown/restart.
	(void)CallbackContext;
	(void)In_DeliveryToken;
}

int APaho_Manager_Sync::MessageArrived(void* CallbackContext, char* TopicName, int TopicLenght, MQTTClient_message* Message)
{
	auto NullTerminatedStringConverter = [](const char* In_Chars) -> FString
		{
			if (!In_Chars)
			{
				return FString();
			}
			auto Converter = StringCast<UTF8CHAR>(In_Chars);
			FString RetVal;
			RetVal.AppendChars(Converter.Get(), Converter.Length());
			return RetVal;
		};

	auto PayloadStringConverter = [](const void* Payload, int PayloadLength) -> FString
		{
			if (!Payload || PayloadLength <= 0)
			{
				return FString();
			}

			const ANSICHAR* PayloadChars = static_cast<const ANSICHAR*>(Payload);
			FUTF8ToTCHAR Converter(PayloadChars, PayloadLength);
			return FString(Converter.Length(), Converter.Get());
		};

	const FString TopicNameStr = NullTerminatedStringConverter(TopicName);
	const FString PayloadStr = Message ? PayloadStringConverter(Message->payload, Message->payloadlen) : FString();

	FJsonObjectWrapper MessageJson;
	const bool bIsJsonOk = MessageJson.JsonObjectFromString(PayloadStr);

	FJsonObjectWrapper Arrived;
	Arrived.JsonObject->SetStringField("TopicName", TopicNameStr);
	Arrived.JsonObject->SetNumberField("TopicLength", TopicLenght);

	if (bIsJsonOk)
	{
		Arrived.JsonObject->SetObjectField("Message", MessageJson.JsonObject);
	}

	else
	{
		Arrived.JsonObject->SetStringField("Message", PayloadStr);
	}

	MQTTClient_freeMessage(&Message);
	MQTTClient_free(TopicName);

	APaho_Manager_Sync* OwnerPtr = static_cast<APaho_Manager_Sync*>(CallbackContext);
	AsyncTask(ENamedThreads::GameThread, [OwnerPtr, Arrived]()
	{
		if (!IsValid(OwnerPtr) ||
			OwnerPtr->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
			OwnerPtr->GetWorld() == nullptr ||
			OwnerPtr->GetWorld()->bIsTearingDown)
		{
			return;
		}

		OwnerPtr->Delegate_Message_Arrived.Broadcast(Arrived);
	});

	return 1;
}

void APaho_Manager_Sync::ConnectionLost(void* CallbackContext, char* Cause)
{
	// Paho may call this while the PIE world or APaho_Manager_Sync UObject is
	// being torn down. The project does not consume this notification, and
	// broadcasting the Blueprint delegate here can race with EndPlay cleanup.
	(void)CallbackContext;
	(void)Cause;
}

#pragma endregion Callbacks
