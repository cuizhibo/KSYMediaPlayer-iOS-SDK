
#import <pthread.h>

/**
 * The KSYEventManager class provides a safe way for inter-thread communications.
 */
@interface KSYEventManager : NSObject

/* Factories */
/**
 * Returns the shared VLCEventManager.  There should only be one instance of this class.
 * \return Shared event manager.
 */
+ (id)sharedManager;

/* Operations */
/**
 * Sends a message to the target's delegate on the main thread.
 * \discussion The main thread is the one in which the main run loop is run, which usually
 * means the one in which the NSApplication object receives events. The method is performed
 * when the main thread runs the run loop in one of the common run loop modes (as specified
 * in the CFRunLoop documentation).
 *
 * The receiver is retained until the call is finished.
 * \param aTarget The target object who's delegate should receive the specified message.
 * \param aSelector A selector that identifies the method to invoke. The method should not
 * have a significant return value and should take a single argument of type NSNotification,
 * or no arguments.
 *
 * See ‚ÄúSelectors‚Äù for a description of the SEL type.
 * \param aNotificiationName The name of the notification that should be sent to the
 * distributed notification center.
 */
- (void)callOnMainThreadDelegateOfObject:(id)aTarget
                      withDelegateMethod:(SEL)aSelector
                    withNotificationName:(NSString *)aNotificationName;

/**
 * Sends a message to the target on the main thread.
 * \discussion The main thread is the one in which the main run loop is run, which usually
 * means the one in which the NSApplication object receives events. The method is performed
 * when the main thread runs the run loop in one of the common run loop modes (as specified
 * in the CFRunLoop documentation).
 *
 * The receiver and arg are retained until the call is finished.
 * \param aTarget The target object who should receive the specified message.
 * \param aSelector A selector that identifies the method to invoke. The method should not
 * have a significant return value and should take a single argument of type id,
 * or no arguments.
 *
 * See ‚ÄúSelectors‚Äù for a description of the SEL type.
 * \param arg The argument to pass in the message.  Pass nil if the method does not take an
 * argument.
 * distributed notification center.
 */
- (void)callOnMainThreadObject:(id)aTarget
                    withMethod:(SEL)aSelector
          withArgumentAsObject:(id)arg;

- (void)cancelCallToObject:(id)target;
@end
