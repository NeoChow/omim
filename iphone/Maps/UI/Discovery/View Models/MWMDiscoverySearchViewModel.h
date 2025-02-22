#import "MWMRatingSummaryViewValueType.h"

NS_ASSUME_NONNULL_BEGIN

@interface MWMDiscoverySearchViewModel : NSObject

@property(nonatomic, readonly) NSString *title;
@property(nonatomic, readonly) NSString *subtitle;
@property(nonatomic, readonly) NSString *distance;
@property(nonatomic, readonly) BOOL isPopular;
@property(nonatomic, readonly) NSString *ratingValue;
@property(nonatomic, readonly) MWMRatingSummaryViewValueType ratingType;

- (instancetype)initWithTitle:(NSString *)title
                     subtitle:(NSString *)subtitle
                     distance:(NSString *)distance
                    isPopular:(BOOL)isPopular
                  ratingValue:(NSString *) ratingValue
                   ratingType:(MWMRatingSummaryViewValueType)ratingType;
@end

NS_ASSUME_NONNULL_END
