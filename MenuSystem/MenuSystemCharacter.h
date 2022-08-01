// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MenuSystemCharacter.generated.h"

UCLASS(config=Game)
class AMenuSystemCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
public:
	AMenuSystemCharacter();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Input)
	float TurnRateGamepad;

protected:

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

public:

	//Pointer to the online session interface
	//IOnlineSessionPtr OnlineSessionInterface;		//This session is meant to hold the session interface for us. But it is undefined and we can't forward declare it since it is type def (if hover over then can see that it has type def written). When a variable is thread safe, that means it has a particular qualities designed to make it safe for multi-threaded code (thus we can't simply forward declare it). When a program uses multiple threads, we have parallel lines of execution going on simultaneously which means that our variable need to behave in a way that we don't get any unintended behaviour with this multiple threads going on. Now because IOnlineSessionPtr is simply a TSharedPtr (if you hover over it you can see that it has a typedef of TSharedPtr) with that Thread Safe option, we can do 2 things to define this (otherwise UE Wont recognize it and we can't forward declare a typedef which in an alias for the type that it shows when we hover over it (itc its a TSharedPtr)): 1. Include the header file for the IOnlineSessionPtr, or simply use the TSharedPtr Smart Pointer Wrapper, which is given down below in this code. That is why we no longer use this line and switch to the code at the bottom. On the lecture it actually proceeds with using the header file...
	TSharedPtr<class IOnlineSession, ESPMode::ThreadSafe> OnlineSessionInterface;			//Here we are reconstructing the type defs on our own such that we dont need to include the header file of this type. So The IOnlinePtr at the top is simply an alias for this TSharedPtr designed to hold an IonlineSession with the thread safe option which we store it in a variable called "OnlineSessionInterface".

protected:
	//This is a function to call when a key in the keyboard is pressed. We set this logic in the blueprint file (third character BP)
	UFUNCTION(BlueprintCallable)
	void CreateGameSession();
	UFUNCTION(BlueprintCallable)
	void JoinGameSession();
	
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);	// this is a callback function that is called when the delegate is triggered.
	void OnFindSessionsComplete(bool bWasSuccessful);							//// this is a callback function that is called when the delegate is triggered.
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);	//this is a callback function that is called when the delegate is triggered. It has 2 parameters: SessionName and an EOnJoinSessionCompleteResult type (apart from 'type' we also have several other input options (L13 9:40), that 'type' is just one of the many input options, essentially this enum exist to tell us whether we have successfully joined the sessions or not). so once triggered, our callback will called and passed in these argument.
private:
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;		//This is the delegate! but Here, it is still just a variable of this type and we still need to construct the delegate, aka define the delegatea and bind it to its callback function ( we do this at the constructor at .cpp file where we initialize the 'CreateSessionCompleteDelegate' variable). Actually this is also a type def and we can't forward declare it, but in this case we are gonna include the header file at the top instead! instead of reconstructing the type defs on our own like the one we did to the OnlineSessionInterface. If we want to reconstruct the type def like the one we did above also can! but we chose to use the header file instead (#include "MenuSystemCharacter.generated.h"). Actually since we have included the header file for the type defs, I no longer need to reconstruct the type def for the OnlineSessionInterface and can go back to using "IOnlineSessionPtr OnlineSessionInterface;" but for demonstration purposes, I just leave it there to show that there are actually 2 ways we can go with when dealing with a type def (where we can't forward declare).
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;		////This is the delegate! but Here, still need to bind to the callback function (was done in the constructor)
	
	//Created a new session search wrapped in a tsharedptr
	TSharedPtr<FOnlineSessionSearch>SessionSearch;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;		////This is the delegate! but Here, still need to bind to the callback function (was done in the constructor)

}
;
