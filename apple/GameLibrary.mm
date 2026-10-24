// SPDX-License-Identifier: MIT
#import "GameLibrary.h"
#import <TargetConditionals.h>
@implementation GameLibrary
+ (NSArray<NSDictionary*>*)entries {
    NSArray* entries = [NSUserDefaults.standardUserDefaults arrayForKey:@"PC98GameFolders"];
    return entries ?: @[];
}
+ (BOOL)rememberURL:(NSURL*)url name:(NSString*)name error:(NSError**)error {
    NSURLBookmarkCreationOptions options = 0;
#if TARGET_OS_OSX
    options = NSURLBookmarkCreationWithSecurityScope;
#endif
    NSData* bookmark = [url bookmarkDataWithOptions:options includingResourceValuesForKeys:nil relativeToURL:nil error:error];
    if (!bookmark) return NO;
    NSMutableArray* entries = [[self entries] mutableCopy];
    NSIndexSet* duplicates = [entries indexesOfObjectsPassingTest:^BOOL(NSDictionary* e, NSUInteger i, BOOL* stop) {
        return [e[@"path"] isEqual:url.path];
    }];
    [entries removeObjectsAtIndexes:duplicates];
    [entries insertObject:@{@"name":name, @"path":url.path, @"bookmark":bookmark} atIndex:0];
    while(entries.count > 20) [entries removeLastObject];
    [NSUserDefaults.standardUserDefaults setObject:entries forKey:@"PC98GameFolders"];
    return YES;
}
+ (NSURL*)resolveEntry:(NSDictionary*)entry error:(NSError**)error {
    BOOL stale = NO;
    NSURLBookmarkResolutionOptions options = NSURLBookmarkResolutionWithoutUI;
#if TARGET_OS_OSX
    options |= NSURLBookmarkResolutionWithSecurityScope;
#endif
    NSURL* url = [NSURL URLByResolvingBookmarkData:entry[@"bookmark"] options:options relativeToURL:nil bookmarkDataIsStale:&stale error:error];
    // Refresh only after the caller has acquired access; never silently fall back to a stale path.
    return url;
}
@end
