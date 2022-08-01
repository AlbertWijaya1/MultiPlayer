// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuSystemCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
//////////////////////////////////////////////////////////////////////////
// AMenuSystemCharacter

AMenuSystemCharacter::AMenuSystemCharacter():
	//binding the delegate to the callback function. @ header file we only created a delegate and a callback function, but still need to bind them to tell that the callback funciton is the function of that delegate.
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),		//Here using the CreateUObject function, we are constructing the delegate object (defining the'CreateSessionCompleteDelegate' delegate) as well as passing in the callback function ('OnCreateSessionComplete') to bind to when the delegate is being triggered (itc when the Session is created will trigger the delegate).  We do this after the : of the constructor to use the member initializing list method of initialize a variable (Syntax: VarName(value you want to initialize the variable to)). ITC we want to initialize our variable CreateSessionCompleteDelegate
	//binding the delegate to the callback function. @ header file we only created a delegate and a callback function, but still need to bind them to tell that the callback funciton is the function of that delegate which is what we're doing here.
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	//binding the delegate to the callback function. @ header file we only created a delegate and a callback function, but still need to bind them to tell that the callback funciton is the function of that delegate which is what we're doing here.
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rate for input
	TurnRateGamepad = 50.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)


	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if(OnlineSubsystem){
		OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();			//GetSessionInterface will return the session interface and return all the functions related to the session interface to be stored in "OnlineSessionInterface"

		if(GEngine){
			GEngine->AddOnScreenDebugMessage(			//This is similar to the UE_LOG but we can customize the color, the duration, and if we want to replace the previous message if we were to print more than one onscreen message
				-1,
				15.f,
				FColor::Blue,
				FString::Printf(TEXT("Found Subsystem %s"), *OnlineSubsystem->GetSubsystemName().ToString())		//GetSystemName() returns an FName but we can convert it to an FString using "ToString()"
			);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AMenuSystemCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AMenuSystemCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AMenuSystemCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &AMenuSystemCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &AMenuSystemCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AMenuSystemCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AMenuSystemCharacter::TouchStopped);
}

void AMenuSystemCharacter::CreateGameSession() 
{		
	// Called when pressing the 1 key

	/* Here we are checking if the online session interface is valid? if not then we simply return out of the function. Then we get the existing session, if one exist with the name "NAME_GameSession", and if that existing session is not a nullptr, we simply destroy the current session with that name, that way we'll be able to create a new session. */
	if(!OnlineSessionInterface.IsValid()){		//IsValid is the function belong to the TSharedPtr wrapper (which is what the onlinesessioninterface is)
		return;		//return early
	}


	//Check if a session has already existed?
	auto ExistingSession = OnlineSessionInterface->GetNamedSession(NAME_GameSession);		//once we start the sessions, we'll be giving them session names. We put Name_GameSession to always be checking to see if the session with such name exist or not.
	
	//if the existing session is null, we destroy the session
	if(ExistingSession != nullptr){
		OnlineSessionInterface->DestroySession(NAME_GameSession);			//destroy the session with the name given inside the bracket
	}
	
	//adding the delegate we define in the constructor to the session interfaces delegate list using the AddOnCreateSessionCompleteDelegate_Handle() function.
	OnlineSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);		//Adding the delegate 'CreateSessionCompleteDelegate' to the OnlineSessionInterface delegate list. Once the successfuly created a session, the callback function that we bound to the delegate (itc its the '&ThisClass::OnCreateSessionComplete' see at the top (at the constructor where we define the delegate to see what callback function we bound to the delegate)) will be executed. So the trigger here is when the the session is created! (other general delegate trigger can be OnHit, OnDamageTaken, etc).

	//Created a new session settings wrapped in a tsharedptr, which we had to use 'MakeSharable()' in order to initialize it with the new FOnlineSessionSettings() object. This is the session settings that we need to passed in to create the sessions (Part of the CreateSession argument below, which we need to complete the argument). And this session settings are also used to configure some of the settings in the sessions which is done down below "SessionSettings->A; SessionSettings->B, SessionSettings->C, etc"
	TSharedPtr<FOnlineSessionSettings>SessionSettings = MakeShareable(new FOnlineSessionSettings());		//the way to initialize a new tsharepointer is by the use of the MakeSharable(new NameOfTheConstructorWe'reCreating) function
	
	//get the first local player in the world and store it in the 'LocalPlayer' variable. We need this to get the preffered unique id which is a function from ULocalPlayer (we need this as a argument of "CreateSession()" below), thus in order to use the function we first need to get the local player stored in ULocalPlayer DT.
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();						//get the first local player from its player controller. This returns a ULocalPlayer, and we need this to get the unique net id which required for the CreateSession argument (The function we need is inside the LocalPlayer).
	
	//Call the "CreateSession()" function and fill in the arguments
	OnlineSessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *SessionSettings);		//This part will create the session. Syntax: CreateSession(FUniqueNetId, SessionName, FOnlineSessionSettings). If the argument don't expect a pointer but what we have is a pointer, we can derefence by using the * sign (like what we did to the SessionSettings which is wrapped in a TSharedPtr and thus we need to dereference it (all TSharedPtr need dereference???)). GetPreferredUniqueNetId()>FUniqueNetIdRepl>FUniqueNetIdWrapper>" * Dereference operator returns a reference to the FUniqueNetId" . That is why we dereference the 'LocalPlayer->GetPreferredUniqueNetId()'


	//configurations for the session settings:

	//Not connecting to LAN match, we want to connect over the internet
	SessionSettings->bIsLANMatch = false;
	//Determines how many player can connect to the game
	SessionSettings->NumPublicConnections = 4;
	//if a session is running, other players can join while that session is running
	SessionSettings->bAllowJoinInProgress = true;
	//When steam setup game session, it uses something called 'presence'. It doesn't just connect any player from around the world to any other player, it has regions and it searches for sessions currently going on at your region of the world. presence is used such that steam will only connect you to other players in the same region. Now we need to use presence in order for our connectiosn to work, thus setting this 'bAllowJoinViaPresence' to true
	SessionSettings->bAllowJoinViaPresence = true;
	//Allows steam to advertise the sessions so other player can find and join that session
	SessionSettings->bShouldAdvertise = true;
	//Allows us to use presence inorder to find sessions going on in our region of the world and presence is used such that steam will only connect you to other players in the same region
	SessionSettings->bUsesPresence = true;
	//If not use this, after building package, it will return "Create Session Failed". This line is troubleshooting! from qna
	SessionSettings->bUseLobbiesIfAvailable = true;
	//Specifying the match type (aka free for all meaning its us against all, or it can be team A vs team B, etc)
	SessionSettings->Set(FName("MatchType"), FString("FreeForAll"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);		//Here, we are setting a key value pair for our session in the session settings (hence the 'SessionSettings->...'), once we found the sessions, we can check that. A Set() function allow us to set a key value pair associated with this session that we can check once we found sessions (Syntax = Set(FName Key, ValueType, EOnlineDataAdvertisementType::Type)). 
}



//this is the callback function that is going to be called by the delegate!
void AMenuSystemCharacter::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful) 
{
	if(bWasSuccessful){						//if we succeffuly created a session

		if(GEngine){
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Blue,
				FString::Printf(TEXT("Success to create session: %s!"), *SessionName.ToString())		//print the sessionname
			);
		}
		UWorld* World = GetWorld();
		if(World){
			World->ServerTravel(FString("/Game/ThirdPerson/Maps/Lobby?listen"));		//travel to the level named the one mentioned in the location inside the FString."?" means that there's an option given to us and we can choose what we want from that option. itc we have the option to choose whether we want the server to be a listen or client and we choose listen.
		}
	}

	else {									//if we failed to create the session
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Red,
			FString(TEXT("Failed to create session!"))
		);
	}
}
//this is the callback function that is going to be called by the delegate!
void AMenuSystemCharacter::JoinGameSession() 
{
	//Find Game Sessions

	/* Here we are checking if the online session interface is valid? if not then we simply return out of the function. Then we get the existing session, if one exist with the name "NAME_GameSession", and if that existing session is not a nullptr, we simply destroy the current session with that name, that way we'll be able to create a new session. */
	if(!OnlineSessionInterface.IsValid()){		//IsValid is the function belong to the TSharedPtr wrapper (which is what the onlinesessioninterface is)
		return;		//return early
	}

	//adding the delegate we define in the constructor to the session interfaces delegate list using the AddOnCreateSessionCompleteDelegate_Handle() function.
	OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);		//Adding the delegate 'FindSessionsCompleteDelegate' to the OnlineSessionInterface delegate list. Once the successfuly created a session, the callback function that we bound to the delegate (itc its the '&ThisClass::OnFindSessionComplete' see at the top (at the constructor where we define the delegate to see what callback function we bound to the delegate)) will be executed. So the trigger here is when "FindSessions()" has completed! (other general delegate trigger can be OnHit, OnDamageTaken, etc).
	//We made SessionSearch as a member variable (aka putting it outside this "OnFindSessionComplete()" function, so we need to make it global for this class in the header file private section) instead of local variable bcs we need to access the function (search result) from this 'SessionSearch' variable, that is why we make it SharePtr and not ShareRef in the first place, is that bcs we want to make this as a variable in order to use the function it stores from its datatype (which is FOnlineSessionSearchResult). Similar to SessionSettings earlier above, we had to use 'MakeSharable()' in order to initialize it with the new FOnlineSessionSearch() object. This is the session search that we need to passed in to find the sessions (Part of the FindSessions() argument below, which we need to complete the argument). And this session settings are also used to configure some of the settings in the sessions which is done down below "SessionSearch->A; SessionSearch->B, SessionSearch->C, etc"
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	//Remember that we are using the DevAppID of 480? (written in the defaultengine.ini). This is shared among lots of people who also are using this DevAppID cos they dont have their own. We set this number to a high value such that there will be a good chance that we find lots of sessions when we search for them. Thus, this will be the sessions used by other developers using the Dev App ID of 480.
	SessionSearch->MaxSearchResults = 10000;
	//we are not connecting using LAN network
	SessionSearch->bIsLanQuery = false;
	//Since we are using presence (aka to know which region are we in? and will only connect you to other players in the same region), we need to set the query settings to make sure that any sessions we find are using presence. if you hover over querysettings, it says that it is used to find matching servers. Syntax: QuerySettings(A Macro (key), set the query search presence to true/false?, Comparison Operator(we passed in an Enum, hence the '::') )
	SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
	
	//get the first local player in the world and store it in the 'LocalPlayer' variable. We need this to get the preffered unique id which is a function from ULocalPlayer (we need this as a argument of "FindSession()" below), thus in order to use the function we first need to get the local player stored in ULocalPlayer DT.
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();						//get the first local player from its player controller. This returns a ULocalPlayer, and we need this to get the unique net id which required for the CreateSession argument (The function we need is inside the LocalPlayer).
	//calling find sessions
	OnlineSessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(),SessionSearch.ToSharedRef());		//different from "CreateSession()", "FindSession()" will need a SharedRef and not SharedPtr. Well, we can actually make it as SharedPtr rightfrom the start but the reason why we make it as a sharedptr is that we later plan on makin this variables on this class so that we can access information on them later.
}

void AMenuSystemCharacter::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result) 
{
	if(!OnlineSessionInterface.IsValid()){		//if this is not (negation symbol !) valid, then return early
		return;
	}

	FString Address;
	if(OnlineSessionInterface->GetResolvedConnectString(NAME_GameSession,  Address)){		// if you hover over "GetResolvedConnectString()" it says that it returns the platform specific connection infomration (like the IP address) for joining the match and it is stored in an FString that we specified in its second parameter (itc the second parameter is an FString called "Address", thus we're storing the platform specific connection information in a variable called "Address"). This function must be called from the delegate join completion (which is what were doing! we are calling this funciton on "void OnJoinSessionComplete()", which will be called once joining the sessions have been completed. Syntax = GetResolvedConnectString(FName for the session name, FString for the connect info which is a reference which mean that the parameter can be overwritten! itc we want to fill in the FString Address with the adress we need). GetResolvedConnectString returns a boolean
		//we only reach here if GetResolvedConnectString returns true (since "GetResolvedConnectString()" is a boolean )
		if(GEngine){
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Yellow,
				FString::Printf(TEXT("Connect String on Address: %s"), *Address)
			);
		}
	 	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
		if(PlayerController){
			PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);					//ClientTravel() function allows us to travel to the IP adress specified in the argument, itc its "Address" which we get from the "GetResolvedConnectString()"" function above. This function returns the platform specific connection information (like the IP address) for joining the match and it is stored in an FString that we specified in its second parameter (itc the second parameter is an FString called "Address", thus we're storing the platform specific connection information in a variable called "Address"). The TravelType::Travel_Absolute will be discussed in future lecture, for now we just gonna use travel absolute.
		}
	}
	
}
void AMenuSystemCharacter::OnFindSessionsComplete(bool bWasSuccessful) 

{

	if(!OnlineSessionInterface.IsValid()){		//if this is not valid, then return early
		return;		//return early

	}
	//we have set SessionSearch as a member variable in the header file, thus able to use it directly wo assigning its DT.	
	//Since SearchResults (Function of sessionsearch pointer) is a TArray, means that we can loop throgh it. We then Loop through the Tarray of FOnlineSessionSearchResult and do something for each result:
	for(auto Result:SessionSearch->SearchResults)
	{
		//Getting the session ID string from the SearchResults function
		FString Id = Result.GetSessionIdStr();
		//Getting the owning username
		FString User = Result.Session.OwningUserName;
		//since we are now getting the data from looping the searchresults, we can also see if it has a key matching value pair that we are looking for (itc its "MatchType")
		FString MatchType;
		Result.Session.SessionSettings.Get(FName("MatchType"), MatchType);	//This Get function will fill in that match type FString if this particular session has the key "MatchType", and if it does, it will access the value and set our local match type variable to that value. So if we have done that, we can then see if that match type value equals "FreeForAll", which is the match type we specify above in the "void CreateGameSession()". Get() function allow us to specify the key for the key value pair to check if it has that key (itc its checking if it has the key named "MatchType"). The value (second argument for the Get() function) is a reference, and a reference inside a function means that we can overwrite that parameter with something! putting void (&A) means that we can overwrite the A variable, whereas not putting & can't... Check utube about 'reference' if still in doubt. ITC, we want to overwrite the "FString MatchType" with whatever the match type we have assigned to the sessions, itc its a freeforall match type ( can be others too! e.g. Team A vs Team B Game, etc)

		//since we are looping the SearchResult in a for loop, we can now print a debug message once it gets its username ('User) and id ('Id).
		if(GEngine){
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Cyan,
				FString::Printf(TEXT("Id: %s, User: %s"), *Id, *User)
			);
		}
		//so if the session we found has the key "MatchType" and its value is "FreeForAll", then we know we are joining a free for all match then it will execute the code below this section (itc we're adding a debug message if the session has the key MatchType and the value is freeforall).
		if(MatchType == FString("FreeForAll")){
			if(GEngine){
				GEngine->AddOnScreenDebugMessage(
					-1,
					15.f,
					FColor::Cyan,
					FString::Printf(TEXT("Joining Match Type: %s"), *MatchType)
				);
			}
			OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate); //Adding the delegate 'JoinSessionCompleteDelegate' to the OnlineSessionInterface delegate list. Once the successfuly created a session, the callback function that we bound to the delegate (itc its the '&ThisClass::OnJoinSessionComplete' see at the top (at the constructor where we define the delegate to see what callback function we bound to the delegate)) will be executed. So the trigger here is when the the session is created! (other general delegate trigger can be OnHit, OnDamageTaken, etc). This callback will be called once the triggered is launced (itc, its after joining session has been completed)
			//get the first local player in the world and store it in the 'LocalPlayer' variable. We need this to get the preffered unique id which is a function from ULocalPlayer (we need this as a argument of "CreateSession()" below), thus in order to use the function we first need to get the local player stored in ULocalPlayer DT.
			const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();						//get the first local player from its player controller. This returns a ULocalPlayer, and we need this to get the unique net id which required for the CreateSession argument (The function we need is inside the LocalPlayer).
			OnlineSessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result);
		}
	}

}

void AMenuSystemCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AMenuSystemCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AMenuSystemCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AMenuSystemCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AMenuSystemCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AMenuSystemCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
