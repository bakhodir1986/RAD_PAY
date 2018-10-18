using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class user_bonusViewModel
    {
        public long id { get; set; }
        public long uid { get; set; }
        public long? balance { get; set; }
        public long? earns { get; set; }
        public int? block { get; set; }
        public string fio { get; set; }
        public string pan { get; set; }
        public string expire { get; set; }
        public DateTime? add_ts { get; set; }
        public long? bonus_card_id { get; set; }
    }
}