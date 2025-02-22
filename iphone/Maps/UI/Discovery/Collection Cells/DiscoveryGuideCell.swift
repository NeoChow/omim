@objc(MWMDiscoveryGuideCell)
final class DiscoveryGuideCell: UICollectionViewCell {
  @IBOutlet var avatar: UIImageView!
  @IBOutlet var titleLabel: UILabel! {
    didSet {
      titleLabel.font = UIFont.medium14()
      titleLabel.textColor = UIColor.blackPrimaryText()
      titleLabel.numberOfLines = 2
    }
  }
  
  @IBOutlet var subtitleLabel: UILabel! {
    didSet {
      subtitleLabel.font = UIFont.regular12()
      subtitleLabel.textColor = UIColor.blackSecondaryText()
      subtitleLabel.numberOfLines = 1
    }
  }
  
  @IBOutlet var proLabel: UILabel! {
    didSet {
      proLabel.font = UIFont.bold12()
      proLabel.textColor = UIColor.white()
      proLabel.backgroundColor = .clear
      proLabel.text = "";
    }
  }
  
  @IBOutlet var proContainer: UIView! {
    didSet {
      proLabel.backgroundColor = UIColor.ratingRed()
    }
  }
  
  @IBOutlet var detailsButton: UIButton! {
    didSet {
      detailsButton.setTitleColor(UIColor.linkBlue(), for: .normal)
      detailsButton.setTitle(L("details"), for: .normal)
    }
  }
  
  typealias OnDetails = () -> Void
  private var onDetails: OnDetails?
  
  override var isHighlighted: Bool {
    didSet {
      UIView.animate(withDuration: kDefaultAnimationDuration,
                     delay: 0,
                     options: [.allowUserInteraction, .beginFromCurrentState],
                     animations: { self.alpha = self.isHighlighted ? 0.3 : 1 },
                     completion: nil)
    }
  }
  
  override func awakeFromNib() {
    super.awakeFromNib()
    layer.borderColor = UIColor.blackDividers().cgColor
  }
  
  override func prepareForReuse() {
    super.prepareForReuse()
    avatar.image = UIImage(named: "img_guide_placeholder")
    titleLabel.text = ""
    subtitleLabel.text = ""
    proLabel.text = ""
    proContainer.isHidden = true
    onDetails = nil
  }
  
  private func setAvatar(_ avatarURL: String?) {
    guard let avatarURL = avatarURL else { return }
    if !avatarURL.isEmpty, let url = URL(string: avatarURL) {
      avatar.image = UIImage(named: "img_guide_placeholder")
      avatar.wi_setImage(with: url, transitionDuration: kDefaultAnimationDuration)
    } else {
      avatar.image = UIImage(named: "img_guide_placeholder")
    }
  }
  
  @objc func config(avatarURL: String?,
                    title: String,
                    subtitle: String,
                    label: String?,
                    onDetails: @escaping OnDetails) {
    setAvatar(avatarURL)
    titleLabel.text = title
    subtitleLabel.text = subtitle
    self.onDetails = onDetails
    guard let label = label, !label.isEmpty else {
      proContainer.isHidden = true
      return
    }
    proLabel.text = label
    proContainer.isHidden = false
  }
  
  @IBAction private func detailsAction() {
    onDetails?()
  }
}
