#pragma once
// Operations on Objects (Actors)

#include "m2uOperation.h"

#include "ActorEditorUtils.h"
#include "UnrealEd.h"
#include "m2uHelper.h"


class Fm2uOpObjectTransform : public Fm2uOperation
{
public:

Fm2uOpObjectTransform( Fm2uOperationManager* Manager = NULL )
	:Fm2uOperation( Manager ){}

	bool Execute( FString Cmd, FString& Result ) override
	{
		const TCHAR* Str = *Cmd;
		bool DidExecute = true;

		if( FParse::Command(&Str, TEXT("TransformObject")))
		{
			Result = TransformObject(Str);
		}

		else
		{
// cannot handle the passed command
			DidExecute = false;
		}

		if( DidExecute )
			return true;
		else
			return false;
	}

	FString TransformObject(const TCHAR* Str)
	{
		const FString ActorName = FParse::Token(Str,0);
		AActor* Actor = NULL;
		//UE_LOG(LogM2U, Log, TEXT("Searching for Actor with name %s"), *ActorName);

		if(!m2uHelper::GetActorByName(*ActorName, &Actor) || Actor == NULL)
		{
			UE_LOG(LogM2U, Log, TEXT("Actor %s not found or invalid."), *ActorName);
			return TEXT("1");
		}

		m2uHelper::SetActorTransformRelativeFromText(Actor, Str);

		GEditor->RedrawLevelEditingViewports();
		return TEXT("Ok");
	}
};


class Fm2uOpObjectName : public Fm2uOperation
{
public:

Fm2uOpObjectName( Fm2uOperationManager* Manager = NULL )
	:Fm2uOperation( Manager ){}

	bool Execute( FString Cmd, FString& Result ) override
	{
		const TCHAR* Str = *Cmd;
		bool DidExecute = true;

		if( FParse::Command(&Str, TEXT("GetFreeName")))
		{
			const FString InName = FParse::Token(Str,0);
			FName FreeName = m2uHelper::GetFreeName(InName);
			Result = FreeName.ToString();
		}

		else if( FParse::Command(&Str, TEXT("RenameObject")))
		{
			const FString ActorName = FParse::Token(Str,0);
			// jump over the next space
			Str = FCString::Strchr(Str,' ');
			if( Str != NULL)
				Str++;
			// the desired new name
			const FString NewName = FParse::Token(Str,0);

			// find the Actor
			AActor* Actor = NULL;
			if(!m2uHelper::GetActorByName(*ActorName, &Actor) || Actor == NULL)
			{
				UE_LOG(LogM2U, Log, TEXT("Actor %s not found or invalid."), *ActorName);
				Result = TEXT("1"); // NOT FOUND
			}

			// try to rename the actor		  
			const FName ResultName = RenameActor(Actor, NewName);
			Result = ResultName.ToString();
		}

		else
		{
// cannot handle the passed command
			DidExecute = false;
		}

		if( DidExecute )
			return true;
		else
			return false;
	}

/**
 * FName RenameActor( AActor* Actor, const FString& Name)
 *
 * Tries to set the Actor's FName to the desired name, while also setting the Label
 * to the exact same name as the FName has resulted in.
 * The returned FName may differ from the desired name, if that was not valid or
 * already in use.
 *
 * @param Actor The Actor which to edit
 * @param Name The desired new name as a string
 *
 * @return FName The resulting ID (and Label-string)
 *
 * The Label is a friendly name that is displayed everywhere in the Editor and it
 * can take special characters the FName can not. The FName is referred to as the
 * object ID in the Editor. Labels need not be unique, the ID must.
 *
 * There are a few functions which are used to set Actor Labels and Names, but all
 * allow a desync between Label and FName, and sometimes don't change the FName
 * at all if deemed not necessary.
 *
 * But we want to be sure that the name we provide as FName is actually set *if*
 * the name is still available.
 * And we want to be sure the Label is set to represent the FName exactly. It might
 * be very confusing to the user if the Actor's "Name" as seen in the Outliner in
 * the Editor is different from the name seen in the Program, but the objects still
 * are considered to have the same name, because the FName is correct, but the
 * Label is desynced.
 *
 * What we have is:
 * 'SetActorLabel', which is the recommended way of changing the name. This function
 * will set the Label immediately and then try to set the ID using the Actor's
 * 'Rename' method, with names generated by 'MakeObjectNameFromActorLabel' or
 * 'MakeUniqueObjectName'. The Actor's Label and ID are not guaranteed to match
 * when using 'SetActorLabel'.
 * 'MakeObjectNameFromActorLabel' will return a name, stripped of all invalid
 * characters. But if the names are the same, and the ID has a number suffix and
 * the label not, the returned name will not be changed.
 * (rename "Chair_5" to "Chair" will return "Chair_5" although I wanted "Chair")
 * So even using 'SetActorLabel' to set the FName to something unique, based
 * on the Label, and then setting the Label to what ID was returned, is not an
 * option, because the ID might not result in what we provided, even though the
 * name is free and valid.
 *
 * TODO:
 * These functions have the option to create unique names within a specified Outer
 * Object, which might be 'unique within a level'. I don't know if we should
 * generally use globally unique names, or how that might change if we use maya-
 * namespaces for levels.
 */
	FName RenameActor( AActor* Actor, const FString& Name)
	{
		// is there still a name, or was it stripped completely (pure invalid name)
		// we don't change the name then. The calling function should check
		// this and maybe print an error-message or so.
		if( GeneratedName.IsEmpty() )
		FString GeneratedName = m2uHelper::MakeValidNameString(Name);
		{
			return Actor->GetFName();
		}
		FName NewFName( *GeneratedName );

		// check if name is "None", NAME_None, that is a valid name to assign
		// but in maya the name will be something like "_110" while here it will
		// be "None" with no number. So althoug renaming "succeeded" the names
		// differ.
		if( NewFName == NAME_None )
		{
			NewFName = FName( *m2uHelper::M2U_GENERATED_NAME );
		}


		// 2. Rename the object

		if( Actor->GetFName() == NewFName )
		{
			// the new name and current name are the same. Either the input was
			// the same, or they differed by invalid chars.
			return Actor->GetFName();
		}

		UObject* NewOuter = NULL; // NULL = use the current Outer
		ERenameFlags RenFlags = REN_DontCreateRedirectors;
		bool bCanRename = Actor->Rename( *NewFName.ToString(), NewOuter, REN_Test | REN_DoNotDirty | REN_NonTransactional | RenFlags );
		if( bCanRename )
		{
			Actor->Rename( *NewFName.ToString(), NewOuter, RenFlags);
		}
		else
		{
			// unable to rename the Actor to that name
			return Actor->GetFName();
		}

		// 3. Get the resulting name
		const FName ResultFName = Actor->GetFName();
		// 4. Set the actor label to represent the ID
		//Actor->ActorLabel = ResultFName.ToString(); // ActorLabel is private :(
		Actor->SetActorLabel(ResultFName.ToString()); // this won't change the ID
		return ResultFName;
	}// FName RenameActor()


};


class Fm2uOpObjectDelete : public Fm2uOperation
{
public:

	Fm2uOpObjectDelete(Fm2uOperationManager* Manager=nullptr)
		:Fm2uOperation(Manager){}

	bool Execute(FString Cmd, FString& Result) override
	{
		const TCHAR* Str = *Cmd;
		bool DidExecute = false;

		if (FParse::Command(&Str, TEXT("DeleteSelected")))
		{
			auto World = GEditor->GetEditorWorldContext().World();
			((UUnrealEdEngine*)GEditor)->edactDeleteSelected(World);

			Result = TEXT("Ok");
			DidExecute = true;
		}

		else if (FParse::Command(&Str, TEXT("DeleteObject")))
		{
			// Deletion of actors in the editor is a dangerous/complex
			// task as actors can be brushes or referenced, levels
			// need to be dirtied and so on there are no "deleteActor"
			// functions in the Editor, only "DeleteSelected".  Since
			// in most cases a deletion is preceded by a selection,
			// and followed by a selection change, we don't bother and
			// just select the object to delete and use the editor
			// function to do it.
			//
			// TODO: maybe we could reselect the previous selection
			//   after the delete op but this is probably in 99% of the
			//   cases not necessary
			GEditor->SelectNone(/*notify=*/false,
								/*deselectBSPSurf=*/true,
		                        /*WarnAboutManyActors=*/false);
			const FString ActorName = FParse::Token(Str,0);
			AActor* Actor = GEditor->SelectNamedActor(*ActorName);
			auto World = GEditor->GetEditorWorldContext().World();
			((UUnrealEdEngine*)GEditor)->edactDeleteSelected(World);

			Result = TEXT("Ok");
			DidExecute = true;
		}

		return DidExecute;
	}
};


class Fm2uOpObjectDuplicate : public Fm2uOperation
{
public:

Fm2uOpObjectDuplicate( Fm2uOperationManager* Manager = NULL )
	:Fm2uOperation( Manager ){}

	bool Execute( FString Cmd, FString& Result ) override
	{
		const TCHAR* Str = *Cmd;
		bool DidExecute = true;

		if( FParse::Command(&Str, TEXT("DuplicateObject")))
		{
			const FString ActorName = FParse::Token(Str,0);
			AActor* OrigActor = NULL;
			AActor* Actor = NULL; // the duplicate
			//UE_LOG(LogM2U, Log, TEXT("Searching for Actor with name %s"), *ActorName);

			// Find the Original to clone
			if(!m2uHelper::GetActorByName(*ActorName, &OrigActor) || OrigActor == NULL)
			{
				UE_LOG(LogM2U, Log, TEXT("Actor %s not found or invalid."), *ActorName);
				Result = TEXT("1"); // original not found
			}

			// jump over the next space to find the name for the Duplicate
			Str = FCString::Strchr(Str,' ');
			if( Str != NULL)
				Str++;

			// the name that is desired for the object
			const FString DupName = FParse::Token(Str,0);

			// TODO: enable transactions
			//const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "DuplicateActors", "Duplicate Actors") );

			// select only the actor we want to duplicate
			GEditor->SelectNone(true, true, false);
			//OrigActor = GEditor->SelectNamedActor(*ActorName); // actor to duplicate
			GEditor->SelectActor(OrigActor, true, false);
			auto World = GEditor->GetEditorWorldContext().World();
			// Do the duplication
			((UUnrealEdEngine*)GEditor)->edactDuplicateSelected(World->GetCurrentLevel(), false);

			// get the new actor (it will be auto-selected by the editor)
			FSelectionIterator It( GEditor->GetSelectedActorIterator() );
			Actor = static_cast<AActor*>( *It );

			if( ! Actor )
				Result = TEXT("4"); // duplication failed?

			// if there are transform parameters in the command, apply them
			m2uHelper::SetActorTransformRelativeFromText(Actor, Str);

			GEditor->RedrawLevelEditingViewports();

			// Try to set the actor's name to DupName
			// NOTE: a unique name was already assigned during the actual duplicate
			// operation, we could just return that name instead and say "the editor
			// changed the name" but if the DupName can be taken, it will save a lot of
			// extra work on the program side which has to find a new name otherwise.
			//GEditor->SetActorLabelUnique( Actor, DupName );
			Fm2uOpObjectName Renamer;
			Renamer.RenameActor(Actor, DupName);

			// get the editor-set name
			const FString AssignedName = Actor->GetFName().ToString();
			// if it is the desired name, everything went fine, if not,
			// send the name as a response to the caller
			if( AssignedName == DupName )
			{
				//Conn->SendResponse(TEXT("0"));
				Result = TEXT("0");
			}
			else
			{
				//Conn->SendResponse(FString::Printf( TEXT("3 %s"), *AssignedName ) );
				Result = FString::Printf( TEXT("3 %s"), *AssignedName );
			}
		}

		else
		{
// cannot handle the passed command
			DidExecute = false;
		}

		if( DidExecute )
			return true;
		else
			return false;
	}
};


class Fm2uOpObjectAdd : public Fm2uOperation
{
public:

Fm2uOpObjectAdd( Fm2uOperationManager* Manager = NULL )
	:Fm2uOperation( Manager ){}

	bool Execute( FString Cmd, FString& Result ) override
	{
		const TCHAR* Str = *Cmd;
		bool DidExecute = true;

		if( FParse::Command(&Str, TEXT("AddActor")))
		{
			Result = AddActor(Str);
		}

		else if( FParse::Command(&Str, TEXT("AddActorBatch")))
		{
			Result = AddActorBatch(Str);
		}

		else
		{
// cannot handle the passed command
			DidExecute = false;
		}

		if( DidExecute )
			return true;
		else
			return false;
	}

/**
   parses a string to interpret what actor to add and what properties to set
   currently only supports name and transform properties
   TODO: add support for other actors like lights and so on

   If the name is already taken, a replace or edit of the existing object is
   possible. If that is not wanted, but the name taken, a new name will be created.
   That name will be returned to the caller. If the name result is not as desired,
   the caller might want to rename the source-object (see object rename functions).
*/
	FString AddActor(const TCHAR* Str)
	{
		FString AssetName = FParse::Token(Str,0);
		const FString ActorName = FParse::Token(Str,0);
		auto World = GEditor->GetEditorWorldContext().World();
		ULevel* Level = World->GetCurrentLevel();
		
		// Parse additional parameters
		bool bEditIfExists = true;
		FParse::Bool(Str, TEXT("EditIfExists="), bEditIfExists);
		// Note: Replacing would happen if the object to create is of a different type 
		// than the one that already has that desired name. 
		// it is very unlikely that in that case not simply a new name can be used
		//bool bReplaceIfExists = false;
		//FParse::Bool(Str, TEXT("ReplaceIfExists="), bReplaceIfExists);

		// check if actor with that name already exists
		// if so, modify or replace it (check for additional parameters for that)
		FName ActorFName = m2uHelper::GetFreeName(ActorName);
		AActor* Actor = NULL;
		if( (ActorFName.ToString() != ActorName) && bEditIfExists )
		{
			// name is taken and we want to edit the object that has the name
			if(m2uHelper::GetActorByName( *ActorName, &Actor))
			{
				UE_LOG(LogM2U, Log, TEXT("Found Actor for editing: %s"), *ActorName);
			}
			else 
				UE_LOG(LogM2U, Warning, TEXT("Name already taken, but no Actor with that name found: %s"), *ActorName);
		}
		else
		{	
			// name was available or we don't want to edit, so create new actor
			Actor = AddNewActorFromAsset(AssetName, Level, ActorFName, false);
		}

		if( Actor == NULL )
		{
			//UE_LOG(LogM2U, Log, TEXT("failed creating from asset"));
			return TEXT("1");
		}

		ActorFName = Actor->GetFName();

		// now we might have transformation data in that string
		// so set that, while we already have that actor
		// (no need in searching it again later
		m2uHelper::SetActorTransformRelativeFromText(Actor, Str);
		// TODO: set other attributes
		// TODO: set asset-reference (mesh) at least if bEdit

		// TODO: we might have other property data in that string
		// we need a function to set light radius and all that

		return ActorFName.ToString();
	}

	/**
	   add multiple actors from the string,
	   expects every line to be a new actor
	*/
	FString AddActorBatch(const TCHAR* Str)
	{
		UE_LOG(LogM2U, Log, TEXT("Batch Add parsing lines"));
		FString Line;
		while( FParse::Line(&Str, Line, 0) )
		{
			if( Line.IsEmpty() )
				continue;
			UE_LOG(LogM2U, Log, TEXT("Read one Line: %s"),*Line);
			AddActor(*Line);
		}
		// TODO: return a list of the created names
		return TEXT("Ok");
	}

	/**
 * Spawn a new Actor in the Level. Automatically find the type of Actor to create 
 * based on the type of Asset. 
 * 
 * @param AssetPath The full path to the asset "/Game/Meshes/MyStaticMesh"
 * @param InLevel The Level to add the Actor to
 * @param Name The Name to assign to the Actor (should be a valid FName) or NAME_None
 * @param bSelectActor Select the Actor after it is created
 *
 * @return The newly created Actor
 *
 * Inspired by the DragDrop functionality of the Viewports, see
 * LevelEditorViewport::AttemptDropObjAsActors and
 * SLevelViewport::HandlePlaceDraggedObjects
 */
	AActor* AddNewActorFromAsset( FString AssetPath, 
								  ULevel* InLevel, 
								  FName Name = NAME_None,
								  bool bSelectActor = true, 
								  EObjectFlags ObjectFlags = RF_Transactional)
	{

		UObject* Asset = m2uAssetHelper::GetAssetFromPath(AssetPath);
		if( Asset == NULL)
			return NULL;

		//UClass* AssetClass = Asset->GetClass();
		
		if( Name == NAME_None)
		{
			//Name = FName(TEXT("GeneratedName"));
			Name = FName( *m2uHelper::M2U_GENERATED_NAME );
		}

		const FAssetData AssetData(Asset);
		FText ErrorMessage;
		AActor* Actor = NULL;	  

		// find the first factory that can create this asset
		for( UActorFactory* ActorFactory : GEditor->ActorFactories )
		{
			if( ActorFactory -> CanCreateActorFrom( AssetData, ErrorMessage) )
			{
#if ENGINE_MINOR_VERSION < 4 // 4.3 etc
				Actor = ActorFactory->CreateActor(Asset, InLevel, FVector(0,0,0),
												  NULL, ObjectFlags, Name);
#else // 4.4
				Actor = ActorFactory->CreateActor(Asset, InLevel, FTransform::Identity,
												  ObjectFlags, Name);
#endif
				if( Actor != NULL)
					break;
			}
		}
		
		if( !Actor )
			return NULL;


		if( bSelectActor )
		{
			GEditor->SelectNone( false, true);
			GEditor->SelectActor( Actor, true, true);
		}
		Actor->InvalidateLightingCache();
		Actor->PostEditChange();

		// The Actor will sometimes receive the Name, but not if it is a blueprint?
		// It will never receive the name as Label, so we set the name explicitly 
		// again here.
		//Actor->SetActorLabel(Actor->GetFName().ToString());
		// For some reason, since 4.3 the factory will always create a class-based name
		// so we have to rename the actor explicitly completely.
		Fm2uOpObjectName Renamer;
		Renamer.RenameActor(Actor, Name.ToString());
		
		return Actor;
	}// AActor* AddNewActorFromAsset()
};


class Fm2uOpObjectParent : public Fm2uOperation
{
public:

Fm2uOpObjectParent( Fm2uOperationManager* Manager = NULL )
	:Fm2uOperation( Manager ){}

	bool Execute( FString Cmd, FString& Result ) override
	{
		const TCHAR* Str = *Cmd;
		bool DidExecute = true;

		if( FParse::Command(&Str, TEXT("ParentChildTo")))
		{
			Result = ParentChildTo(Str);
		}

		else
		{
// cannot handle the passed command
			DidExecute = false;
		}

		if( DidExecute )
			return true;
		else
			return false;
	}

	FString ParentChildTo(const TCHAR* Str)
	{
		const FString ChildName = FParse::Token(Str,0);
		Str = FCString::Strchr(Str,' ');
		FString ParentName;
		if( Str != NULL) // there may be a parent name present
		{
			Str++;
			if( *Str != '\0' ) // there was a space, but no name after that
			{
				ParentName = FParse::Token(Str,0);
			}
		}

		AActor* ChildActor = NULL;
		if(!m2uHelper::GetActorByName(*ChildName, &ChildActor) || ChildActor == NULL)
		{
			UE_LOG(LogM2U, Log, TEXT("Actor %s not found or invalid."), *ChildName);
			return TEXT("1");
		}

		// TODO: enable transaction?
		//const FScopedTransaction Transaction( NSLOCTEXT("Editor", "UndoAction_PerformAttachment", "Attach actors") );

		// parent to world, aka "detach"
		if( ParentName.Len() < 1) // no valid parent name
		{
			USceneComponent* ChildRoot = ChildActor->GetRootComponent();
			if(ChildRoot->GetAttachParent() != NULL)
			{
				UE_LOG(LogM2U, Log, TEXT("Parenting %s the World."), *ChildName);
				AActor* OldParentActor = ChildRoot->GetAttachParent()->GetOwner();
				OldParentActor->Modify();
				ChildRoot->DetachFromParent(true);
				//ChildActor->SetFolderPath(OldParentActor->GetFolderPath());

				GEngine->BroadcastLevelActorDetached(ChildActor, OldParentActor);
			}
			return TEXT("0");
		}

		AActor* ParentActor = NULL;
		if(!m2uHelper::GetActorByName(*ParentName, &ParentActor) || ParentActor == NULL)
		{
			UE_LOG(LogM2U, Log, TEXT("Actor %s not found or invalid."), *ParentName);
			return TEXT("1");
		}
		if( ParentActor == ChildActor ) // can't parent actor to itself
		{
			return TEXT("1");
		}
		// parent to other actor, aka "attach"
		UE_LOG(LogM2U, Log, TEXT("Parenting %s to %s."), *ChildName, *ParentName);
		GEditor->ParentActors( ParentActor, ChildActor, NAME_None);

		return TEXT("0");
	}

};
